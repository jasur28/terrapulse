// tpacq — TerraPulse acquisition daemon.
// Reads one accelerometer from a serial port and publishes each sample to the
// bus as a `raw.<station>.<object>.<sensor>` message (x/y/z carried in the CBOR
// header). No GUI. This is the standalone replacement for the in-app serial path.
//
// --sim: no hardware needed — generates a synthetic waveform (structural sine +
// noise) at --rate Hz. Handy for pipeline/broker tests and demos.

#include "serial/SerialStreamReceiver.h"
#include "bus/Bus.h"
#include "bus/Soh.h"
#include "bus/Master.h"
#include "mseed/TdsArchive.h"

#include <QCoreApplication>
#include <memory>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace { struct Rec { qint64 t; double x, y, z; }; }

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpacq");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse acquisition daemon");
    parser.addHelpOption();
    QCommandLineOption portOpt   ({"p", "port"},   "Serial port (e.g. COM6)", "port");
    QCommandLineOption masterOpt ({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    QCommandLineOption queueOpt  ("queue", "Target queue: production | playback", "name", "production");
    QCommandLineOption simOpt    ("sim", "No hardware: publish a synthetic waveform instead of reading serial");
    QCommandLineOption rateOpt   ("rate", "Sample rate (Hz) for --sim / --replay", "hz", "200");
    QCommandLineOption replayOpt ("replay", "Replay a recorded CSV (t_ms,x,y,z) — usually to --queue playback", "file");
    QCommandLineOption historicOpt("historic", "Replay keeps original timestamps (default: retime to now)");
    QCommandLineOption speedOpt  ("speed", "Replay speed factor", "x", "1");
    QCommandLineOption recordOpt ("record", "Also append every published sample to a CSV (t_ms,x,y,z)", "file");
    QCommandLineOption archiveOpt("archive", "Also archive raw waveforms as miniSEED into this TDS directory", "dir");
    QCommandLineOption baudOpt   ("baud",    "Baud rate", "rate", "460800");
    QCommandLineOption stationOpt("station", "Station id", "id", "1");
    QCommandLineOption objectOpt ("object",  "Object id",  "id", "1");
    QCommandLineOption sensorOpt ("sensor",  "Sensor id",  "id", "1");
    parser.addOptions({portOpt, masterOpt, queueOpt, simOpt, rateOpt, replayOpt, historicOpt,
                       speedOpt, recordOpt, archiveOpt, baudOpt, stationOpt, objectOpt, sensorOpt});
    parser.process(app);

    const quint32 station = parser.value(stationOpt).toUInt();
    const quint32 object  = parser.value(objectOpt).toUInt();
    const quint32 sensor  = parser.value(sensorOpt).toUInt();
    const bool useSim     = parser.isSet(simOpt);
    const bool useReplay  = parser.isSet(replayOpt);
    const auto queue      = tp::master::queueFromName(parser.value(queueOpt).toStdString());
    const std::string endpoint = tp::master::in(parser.value(masterOpt).toStdString(), queue);
    const std::string topic =
        "raw." + std::to_string(station) + "." + std::to_string(object) + "." + std::to_string(sensor);

    // Publish to the tpmaster broker (XSUB frontend).
    tp::Publisher pub(endpoint, /*bind=*/false);

    // Optional CSV recorder.
    QFile       recFile(parser.value(recordOpt));
    QTextStream recOut;
    if (parser.isSet(recordOpt)) {
        if (recFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            recOut.setDevice(&recFile);
        else
            std::printf("[tpacq] cannot open --record file '%s'\n",
                        parser.value(recordOpt).toUtf8().constData());
    }

    // Optional miniSEED TDS archive (raw waveforms on disk — feeds review/replay).
    std::unique_ptr<tp::mseed::TdsArchive> tds;
    if (parser.isSet(archiveOpt))
        tds = std::make_unique<tp::mseed::TdsArchive>(parser.value(archiveOpt).toStdString());

    quint64 published = 0;
    // Single publish path shared by every source.
    auto publishSample = [&](qint64 t, double x, double y, double z, quint64 seq, quint32 rate) {
        if (recOut.device())
            recOut << t << ',' << x << ',' << y << ',' << z << '\n';
        if (tds) {                       // gal -> integer counts (x10000), 3 axes
            tds->addSample(object, sensor, 0, static_cast<int32_t>(std::lround(x * 10000.0)), t, rate);
            tds->addSample(object, sensor, 1, static_cast<int32_t>(std::lround(y * 10000.0)), t, rate);
            tds->addSample(object, sensor, 2, static_cast<int32_t>(std::lround(z * 10000.0)), t, rate);
        }
        QVariantMap h;
        h["v"]          = 1;
        h["type"]       = "raw";
        h["station"]    = station;
        h["object"]     = object;
        h["sensor"]     = sensor;
        h["t"]          = static_cast<qlonglong>(t);
        h["x"]          = x;
        h["y"]          = y;
        h["z"]          = z;
        h["seq"]        = static_cast<qulonglong>(seq);
        h["sampleRate"] = rate;

        tp::BusMessage msg;
        msg.topic  = topic;
        msg.header = tp::BusMessage::encodeHeader(h);
        // payload empty: a single raw sample fits entirely in the header.
        pub.publish(msg);
        ++published;
    };

    // ── Serial source ────────────────────────────────────────────────────────
    SerialStreamReceiver serial;
    serial.setBaudRate(parser.value(baudOpt).toInt());
    QObject::connect(&serial, &SerialStreamReceiver::sampleReceived,
                     [&](const QVariantMap& s) {
        publishSample(s.value("timestampMs").toLongLong(),
                      s.value("x").toDouble(), s.value("y").toDouble(), s.value("z").toDouble(),
                      s.value("sequence").toULongLong(), s.value("sampleRate").toUInt());
    });

    // ── Synthetic source (--sim) ─────────────────────────────────────────────
    QTimer simTimer;
    const quint32 simRate = std::max(1u, parser.value(rateOpt).toUInt());
    if (useSim) {
        const int    tickMs        = 10;                                    // emit in small batches
        const int    perTick       = std::max(1, int(simRate) * tickMs / 1000);
        static quint64 seq = 0;
        static double  ph  = 0.0;
        const double   f   = 3.1;                                           // structural tone (Hz)
        const double   dph = 2.0 * M_PI * f / double(simRate);
        QObject::connect(&simTimer, &QTimer::timeout, [=, &publishSample]() mutable {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            for (int i = 0; i < perTick; ++i) {
                const double n = double(std::rand() % 1000 - 500) / 1000.0; // +/-0.5 noise
                const double x = 5.0 * std::sin(ph)            + 0.8 * n;
                const double y = 4.0 * std::sin(ph + 1.7)      + 0.8 * n;
                const double z = 1000.0 + 2.0 * std::sin(ph)   + 0.5 * n;   // ~1g bias on Z
                publishSample(now, x, y, z, ++seq, simRate);
                ph += dph;
                if (ph > 2.0 * M_PI) ph -= 2.0 * M_PI;
            }
        });
        simTimer.start(tickMs);
    }

    // ── Replay source (--replay file) ─────────────────────────────────────────
    QTimer replayTimer;
    std::vector<Rec> recs;
    if (useReplay) {
        QFile f(parser.value(replayOpt));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                const QStringList p = in.readLine().split(',');
                if (p.size() >= 4)
                    recs.push_back({ p[0].toLongLong(), p[1].toDouble(), p[2].toDouble(), p[3].toDouble() });
            }
        }
        const bool    historic = parser.isSet(historicOpt);
        const double  speed    = std::max(0.01, parser.value(speedOpt).toDouble());
        const quint32 rate     = std::max(1u, parser.value(rateOpt).toUInt());
        const int     tickMs   = 10;
        const int     perTick  = std::max(1, int(std::llround(rate * speed * tickMs / 1000.0)));
        static std::size_t idx = 0;
        static quint64 seq = 0;
        QObject::connect(&replayTimer, &QTimer::timeout, [=, &publishSample, &recs, &replayTimer]() mutable {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            for (int i = 0; i < perTick && idx < recs.size(); ++i, ++idx) {
                const Rec& r = recs[idx];
                publishSample(historic ? r.t : now, r.x, r.y, r.z, ++seq, rate);
            }
            if (idx >= recs.size()) {
                std::printf("[tpacq] replay done (%zu samples)\n", recs.size());
                std::fflush(stdout);
                replayTimer.stop();
            }
        });
        // Start after a short delay so subscribers connect first (PUB/SUB slow-joiner);
        // a one-shot replay burst could otherwise finish before subscriptions propagate.
        QTimer::singleShot(1300, [&replayTimer, tickMs]() { replayTimer.start(tickMs); });
    }

    const char* srcLabel = useReplay ? "replay" : useSim ? "sim" : "serial";

    // STATUS heartbeat -> tpmaster/tpmm.
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, [&]() {
        QVariantMap c;
        c["published"] = static_cast<qulonglong>(published);
        c["packets"]   = static_cast<qulonglong>(serial.packetCount());
        c["bad"]       = static_cast<qulonglong>(serial.badPacketCount());
        c["src"]       = srcLabel;
        c["queue"]     = tp::master::queueName(queue);
        pub.publish(tp::sohMessage("tpacq", startMs, c));
    });
    heartbeat.start(2000);

    QTimer stats;
    QObject::connect(&stats, &QTimer::timeout, [&]() {
        if (recOut.device()) recOut.flush();
        if (tds) tds->flushAll();
        if (useSim || useReplay) {
            std::printf("[tpacq] %s published=%llu  queue=%s\n",
                        srcLabel, static_cast<unsigned long long>(published),
                        tp::master::queueName(queue));
        } else {
            std::printf("[tpacq] connected=%d bytes=%llu published=%llu packets=%llu bad=%llu\n",
                        serial.isConnected() ? 1 : 0,
                        static_cast<unsigned long long>(serial.bytesReceived()),
                        static_cast<unsigned long long>(published),
                        static_cast<unsigned long long>(serial.packetCount()),
                        static_cast<unsigned long long>(serial.badPacketCount()));
        }
        std::fflush(stdout);
    });
    stats.start(2000);

    const QString port = parser.value(portOpt);
    if (useReplay) {
        std::printf("[tpacq] REPLAY '%s' (%s)  ->  topic '%s'  via tpmaster/%s %s\n",
                    parser.value(replayOpt).toUtf8().constData(),
                    parser.isSet(historicOpt) ? "historic" : "realtime",
                    topic.c_str(), tp::master::queueName(queue), endpoint.c_str());
    } else if (useSim) {
        std::printf("[tpacq] SIM %u Hz  ->  topic '%s'  via tpmaster/%s %s\n",
                    simRate, topic.c_str(), tp::master::queueName(queue), endpoint.c_str());
    } else if (port.isEmpty()) {
        std::printf("[tpacq] no --port given. Available ports: %s\n",
                    serial.availablePorts().join(", ").toUtf8().constData());
    } else if (serial.connectPort(port)) {
        std::printf("[tpacq] %s @ %d baud  ->  topic '%s'  via tpmaster %s\n",
                    port.toUtf8().constData(), serial.baudRate(), topic.c_str(), endpoint.c_str());
    } else {
        std::printf("[tpacq] failed to open %s: %s\n",
                    port.toUtf8().constData(),
                    serial.errorText().toUtf8().constData());
    }
    std::fflush(stdout);

    return app.exec();
}
