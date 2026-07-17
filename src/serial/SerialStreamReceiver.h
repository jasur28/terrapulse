#pragma once

#include <QObject>
#include <QSerialPort>
#include <QVariantMap>

class SerialStreamReceiver : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY availablePortsChanged)
    Q_PROPERTY(QString portName READ portName NOTIFY portNameChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(qulonglong packetCount READ packetCount NOTIFY statsChanged)
    Q_PROPERTY(qulonglong badPacketCount READ badPacketCount NOTIFY statsChanged)
    Q_PROPERTY(QString lastStreamId READ lastStreamId NOTIFY sampleChanged)
    Q_PROPERTY(QString lastTimestamp READ lastTimestamp NOTIFY sampleChanged)
    Q_PROPERTY(double lastX READ lastX NOTIFY sampleChanged)
    Q_PROPERTY(double lastY READ lastY NOTIFY sampleChanged)
    Q_PROPERTY(double lastZ READ lastZ NOTIFY sampleChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    explicit SerialStreamReceiver(QObject* parent = nullptr);

    QStringList availablePorts() const { return m_availablePorts; }
    QString portName() const;
    int baudRate() const { return m_baudRate; }
    bool isConnected() const { return m_serial.isOpen(); }
    qulonglong packetCount() const { return m_packetCount; }
    qulonglong badPacketCount() const { return m_badPacketCount; }
    qulonglong bytesReceived() const { return m_bytesReceived; }
    QString lastStreamId() const { return m_lastStreamId; }
    QString lastTimestamp() const { return m_lastTimestamp; }
    double lastX() const { return m_lastX; }
    double lastY() const { return m_lastY; }
    double lastZ() const { return m_lastZ; }
    QString errorText() const { return m_errorText; }

    void setBaudRate(int baudRate);

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE bool connectPort(const QString& name);
    Q_INVOKABLE void disconnectPort();

signals:
    void availablePortsChanged();
    void portNameChanged();
    void baudRateChanged();
    void connectedChanged();
    void statsChanged();
    void sampleChanged();
    void sampleReceived(const QVariantMap& sample);
    void errorTextChanged(const QString& text);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    static constexpr int    PacketLen    = 43;
    static constexpr quint8 ProtoVersion = 1;   // byte[2] — must match STM32 firmware
    static constexpr quint8 ProtoType    = 1;   // byte[3] — must match STM32 firmware

    void parseBuffer();
    bool parsePacket(const QByteArray& packet, QVariantMap* out);
    static quint16 readU16(const uchar* p);
    static quint32 readU32(const uchar* p);
    static qint32 readI32(const uchar* p);
    static quint16 crc16Ccitt(const uchar* data, int len);

    QSerialPort m_serial;
    QByteArray m_buffer;
    QStringList m_availablePorts;
    int m_baudRate = 460800;
    qulonglong m_packetCount = 0;
    qulonglong m_badPacketCount = 0;
    qulonglong m_bytesReceived = 0;
    QString m_lastStreamId = "-";
    QString m_lastTimestamp = "-";
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    double m_lastZ = 0.0;
    QString m_errorText;
};
