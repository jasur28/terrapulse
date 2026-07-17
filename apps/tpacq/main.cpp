// tpacq — TerraPulse acquisition daemon.
// Reads one accelerometer from a serial port and publishes each sample to the
// bus as a `raw.<station>.<object>.<sensor>` message (x/y/z carried in the CBOR
// header). No GUI. This is the standalone replacement for the in-app serial path.

#include "serial/SerialStreamReceiver.h"
#include "bus/Bus.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpacq");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse acquisition daemon");
    parser.addHelpOption();
    QCommandLineOption portOpt   ({"p", "port"},  "Serial port (e.g. COM6)", "port");
    QCommandLineOption pubOpt    ({"b", "pub"},   "PUB endpoint", "endpoint", "tcp://127.0.0.1:5556");
    QCommandLineOption baudOpt   ("baud",    "Baud rate", "rate", "460800");
    QCommandLineOption stationOpt("station", "Station id", "id", "1");
    QCommandLineOption objectOpt ("object",  "Object id",  "id", "1");
    QCommandLineOption sensorOpt ("sensor",  "Sensor id",  "id", "1");
    parser.addOptions({portOpt, pubOpt, baudOpt, stationOpt, objectOpt, sensorOpt});
    parser.process(app);

    const quint32 station = parser.value(stationOpt).toUInt();
    const quint32 object  = parser.value(objectOpt).toUInt();
    const quint32 sensor  = parser.value(sensorOpt).toUInt();
    const std::string endpoint = parser.value(pubOpt).toStdString();
    const std::string topic =
        "raw." + std::to_string(station) + "." + std::to_string(object) + "." + std::to_string(sensor);

    tp::Publisher pub(endpoint);

    SerialStreamReceiver serial;
    serial.setBaudRate(parser.value(baudOpt).toInt());

    quint64 published = 0;
    QObject::connect(&serial, &SerialStreamReceiver::sampleReceived,
                     [&](const QVariantMap& s) {
        QVariantMap h;
        h["v"]       = 1;
        h["type"]    = "raw";
        h["station"] = station;
        h["object"]  = object;
        h["sensor"]  = sensor;
        h["t"]          = s.value("timestampMs");
        h["x"]          = s.value("x");
        h["y"]          = s.value("y");
        h["z"]          = s.value("z");
        h["seq"]        = s.value("sequence");
        h["sampleRate"] = s.value("sampleRate");

        tp::BusMessage msg;
        msg.topic  = topic;
        msg.header = tp::BusMessage::encodeHeader(h);
        // payload empty: a single raw sample fits entirely in the header.
        pub.publish(msg);
        ++published;
    });

    QTimer stats;
    QObject::connect(&stats, &QTimer::timeout, [&]() {
        std::printf("[tpacq] connected=%d bytes=%llu published=%llu packets=%llu bad=%llu\n",
                    serial.isConnected() ? 1 : 0,
                    static_cast<unsigned long long>(serial.bytesReceived()),
                    static_cast<unsigned long long>(published),
                    static_cast<unsigned long long>(serial.packetCount()),
                    static_cast<unsigned long long>(serial.badPacketCount()));
        std::fflush(stdout);
    });
    stats.start(2000);

    const QString port = parser.value(portOpt);
    if (port.isEmpty()) {
        std::printf("[tpacq] no --port given. Available ports: %s\n",
                    serial.availablePorts().join(", ").toUtf8().constData());
    } else if (serial.connectPort(port)) {
        std::printf("[tpacq] %s @ %d baud  ->  topic '%s'  on %s\n",
                    port.toUtf8().constData(), serial.baudRate(),
                    topic.c_str(), endpoint.c_str());
    } else {
        std::printf("[tpacq] failed to open %s: %s\n",
                    port.toUtf8().constData(),
                    serial.errorText().toUtf8().constData());
    }
    std::fflush(stdout);

    return app.exec();
}
