// tpproc — TerraPulse processing daemon.
// Subscribes to raw samples, runs the analysis + history pipeline, and publishes
// SAF/SHF results on the bus. No GUI. This is the standalone replacement for the
// analysis that used to live inside the Qt app.

#include "proc/ProcPipeline.h"
#include "bus/Bus.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <cstdio>
#include <string>

static std::string topicFor(const char* kind, const QVariantMap& m, bool withAxis) {
    std::string t = std::string(kind) + "."
        + std::to_string(m.value("stationId").toUInt()) + "."
        + std::to_string(m.value("objectId").toUInt())  + "."
        + std::to_string(m.value("sensorId").toUInt());
    if (withAxis)
        t += "." + std::to_string(m.value("component").toInt());
    return t;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpproc");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse processing daemon");
    parser.addHelpOption();
    QCommandLineOption subOpt({"s", "sub"}, "Raw input (SUB) endpoint", "endpoint", "tcp://127.0.0.1:5556");
    QCommandLineOption pubOpt({"b", "pub"}, "Result (PUB) endpoint",    "endpoint", "tcp://127.0.0.1:5557");
    QCommandLineOption winOpt("window", "Samples per analysis window", "n", "100");
    parser.addOptions({subOpt, pubOpt, winOpt});
    parser.process(app);

    const std::string subEndpoint = parser.value(subOpt).toStdString();
    const std::string pubEndpoint = parser.value(pubOpt).toStdString();

    tp::Publisher  pub(pubEndpoint);
    tp::Subscriber sub(subEndpoint);
    sub.subscribe("raw.");

    tp::ProcPipeline pipe;
    pipe.setWindowSize(parser.value(winOpt).toInt());

    quint64 safOut = 0, shfOut = 0;
    pipe.onSaf = [&](const QVariantMap& m) {
        tp::BusMessage msg;
        msg.topic  = topicFor("saf", m, /*withAxis=*/true);
        msg.header = tp::BusMessage::encodeHeader(m);
        pub.publish(msg);
        ++safOut;
    };
    pipe.onShf = [&](const QVariantMap& m) {
        tp::BusMessage msg;
        msg.topic  = topicFor("shf", m, /*withAxis=*/false);
        msg.header = tp::BusMessage::encodeHeader(m);
        pub.publish(msg);
        ++shfOut;
    };

    quint64 rawIn = 0;
    QTimer pollTimer;
    QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
        for (int i = 0; i < 8000; ++i) {
            auto m = sub.receive(0);
            if (!m) break;
            const QVariantMap h = tp::BusMessage::decodeHeader(m->header);
            pipe.addSample(h.value("station").toUInt(),
                           h.value("object").toUInt(),
                           h.value("sensor").toUInt(),
                           h.value("x").toDouble(),
                           h.value("y").toDouble(),
                           h.value("z").toDouble(),
                           h.value("t").toLongLong(),
                           h.value("sampleRate").toUInt());
            ++rawIn;
        }
    });
    pollTimer.start(5);

    QTimer stats;
    QObject::connect(&stats, &QTimer::timeout, [&]() {
        std::printf("[tpproc] rawIn=%llu windows=%d saf=%llu shf=%llu\n",
                    static_cast<unsigned long long>(rawIn),
                    pipe.windowsProcessed(),
                    static_cast<unsigned long long>(safOut),
                    static_cast<unsigned long long>(shfOut));
        std::fflush(stdout);
    });
    stats.start(2000);

    std::printf("[tpproc] raw <- %s   saf/shf -> %s   window=%d\n",
                subEndpoint.c_str(), pubEndpoint.c_str(), pipe.windowSize());
    std::fflush(stdout);

    return app.exec();
}
