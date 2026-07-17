#include "serial/SerialStreamReceiver.h"

#include <QDateTime>
#include <QSerialPortInfo>
#include <QThread>

SerialStreamReceiver::SerialStreamReceiver(QObject* parent)
    : QObject(parent)
{
    connect(&m_serial, &QSerialPort::readyRead, this, &SerialStreamReceiver::onReadyRead);
    connect(&m_serial, &QSerialPort::errorOccurred, this, &SerialStreamReceiver::onErrorOccurred);
    refreshPorts();
}

QString SerialStreamReceiver::portName() const
{
    return m_serial.portName();
}

void SerialStreamReceiver::setBaudRate(int baudRate)
{
    if (baudRate == m_baudRate) return;
    m_baudRate = baudRate;
    if (m_serial.isOpen()) {
        m_serial.setBaudRate(m_baudRate);
    }
    emit baudRateChanged();
}

void SerialStreamReceiver::refreshPorts()
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : infos) {
        ports.push_back(info.portName());
    }
    if (ports == m_availablePorts) return;
    m_availablePorts = ports;
    emit availablePortsChanged();
}

bool SerialStreamReceiver::connectPort(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        m_errorText = "COM port is empty";
        emit errorTextChanged(m_errorText);
        return false;
    }

    if (m_serial.isOpen()) {
        m_serial.close();
        emit connectedChanged();
    }

    m_buffer.clear();
    m_packetCount = 0;
    m_badPacketCount = 0;
    m_bytesReceived = 0;
    emit statsChanged();

    m_serial.setPortName(name.trimmed());
    m_serial.setBaudRate(m_baudRate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial.open(QIODevice::ReadOnly)) {
        m_errorText = m_serial.errorString(); emit errorTextChanged(m_errorText);
        emit portNameChanged();
        emit connectedChanged();
        return false;
    }

    // Kick the device: pulse DTR/RTS low->high. Devices that gate streaming on a
    // fresh terminal connection start sending; if DTR is wired to NRST, this
    // resets the MCU so it reboots and streams again. One-time 60 ms at connect.
    m_serial.setDataTerminalReady(false);
    m_serial.setRequestToSend(false);
    QThread::msleep(60);
    m_serial.setDataTerminalReady(true);
    m_serial.setRequestToSend(true);

    emit portNameChanged();
    emit connectedChanged();
    m_errorText = QString();
    emit errorTextChanged(m_errorText);
    return true;
}

void SerialStreamReceiver::disconnectPort()
{
    if (!m_serial.isOpen()) return;
    m_serial.close();
    m_buffer.clear();
    emit connectedChanged();
    emit portNameChanged();
}

void SerialStreamReceiver::onReadyRead()
{
    const QByteArray chunk = m_serial.readAll();
    m_bytesReceived += static_cast<qulonglong>(chunk.size());
    m_buffer += chunk;
    parseBuffer();
}

void SerialStreamReceiver::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;
    m_errorText = m_serial.errorString(); emit errorTextChanged(m_errorText);

    // Device removed or fatal I/O error — close and notify UI
    if (error == QSerialPort::ResourceError ||
        error == QSerialPort::DeviceNotFoundError ||
        error == QSerialPort::PermissionError) {
        m_serial.close();
        m_buffer.clear();
        emit connectedChanged();
        emit portNameChanged();
    }
}

void SerialStreamReceiver::parseBuffer()
{
    while (m_buffer.size() >= PacketLen) {
        const int start = m_buffer.indexOf("MS");
        if (start < 0) {
            m_buffer = m_buffer.right(1);
            return;
        }
        if (start > 0) {
            m_buffer.remove(0, start);
        }
        if (m_buffer.size() < PacketLen) return;

        const QByteArray packet = m_buffer.left(PacketLen);
        m_buffer.remove(0, PacketLen);

        QVariantMap sample;
        if (!parsePacket(packet, &sample)) {
            ++m_badPacketCount;
            emit statsChanged();
            continue;
        }

        ++m_packetCount;
        m_lastStreamId = sample.value("streamId").toString();
        m_lastTimestamp = sample.value("timestamp").toString();
        m_lastX = sample.value("x").toDouble();
        m_lastY = sample.value("y").toDouble();
        m_lastZ = sample.value("z").toDouble();

        emit sampleChanged();
        emit statsChanged();
        emit sampleReceived(sample);
    }
}

bool SerialStreamReceiver::parsePacket(const QByteArray& packet, QVariantMap* out)
{
    if (packet.size() != PacketLen || !out) return false;

    const auto* p = reinterpret_cast<const uchar*>(packet.constData());
    if (p[0] != 'M' || p[1] != 'S') return false;
    if (p[2] != ProtoVersion || p[3] != ProtoType) return false;
    if (readU16(p + 4) != PacketLen) return false;

    const quint16 expected = readU16(p + 41);
    const quint16 actual = crc16Ccitt(p, PacketLen - 2);
    if (expected != actual) return false;

    const quint32 sequence = readU32(p + 6);
    const quint16 sampleRate = readU16(p + 10);
    const quint16 year = readU16(p + 12);
    const int month = p[14];
    const int day = p[15];
    const int hour = p[16];
    const int minute = p[17];
    const int second = p[18];
    const int millisecond = readU16(p + 19);
    const int streamLen = qstrnlen(reinterpret_cast<const char*>(p + 21), 8);
    const QString streamId = QString::fromLatin1(reinterpret_cast<const char*>(p + 21), streamLen);

    const QDate date(year, month, day);
    const QTime time(hour, minute, second, millisecond);
    const QDateTime stamp(date, time);

    const double x = static_cast<double>(readI32(p + 29)) / 10000.0;
    const double y = static_cast<double>(readI32(p + 33)) / 10000.0;
    const double z = static_cast<double>(readI32(p + 37)) / 10000.0;

    (*out)["sequence"] = static_cast<qulonglong>(sequence);
    (*out)["sampleRate"] = sampleRate;
    (*out)["timestampMs"] = stamp.toMSecsSinceEpoch();
    (*out)["timestamp"] = stamp.toString("yyyy-MM-dd HH:mm:ss.zzz");
    (*out)["streamId"] = streamId;
    (*out)["x"] = x;
    (*out)["y"] = y;
    (*out)["z"] = z;
    return true;
}

quint16 SerialStreamReceiver::readU16(const uchar* p)
{
    return static_cast<quint16>(p[0]) | (static_cast<quint16>(p[1]) << 8);
}

quint32 SerialStreamReceiver::readU32(const uchar* p)
{
    return static_cast<quint32>(p[0])
        | (static_cast<quint32>(p[1]) << 8)
        | (static_cast<quint32>(p[2]) << 16)
        | (static_cast<quint32>(p[3]) << 24);
}

qint32 SerialStreamReceiver::readI32(const uchar* p)
{
    return static_cast<qint32>(readU32(p));
}

quint16 SerialStreamReceiver::crc16Ccitt(const uchar* data, int len)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < len; ++i) {
        crc ^= static_cast<quint16>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                 : static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}
