// tpqc — data-quality monitor (SeisComp scqc analog). Watches the raw stream and
// reports, per sensor, whether the DATA can be trusted: gaps, latency, sample-rate
// drift, spikes, and availability. A structural verdict is only as good as the
// data behind it, so these metrics gate operator confidence.
//
// First module running on tp::client::Application: the base class owns the broker
// connection, the message pump and the heartbeat, so this file contains only the
// quality logic.
//
//   tpqc [--master host] [--queue name] [--period-sec 5]

#include "terrapulse/client/application.h"
#include "terrapulse/messaging/rawsamples.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QHash>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

struct Qc {
    quint32 station = 1;
    quint64 samples = 0;        // received since last report
    quint64 gaps    = 0;        // timestamp jumps beyond one sample period
    quint64 spikes  = 0;        // |a| beyond spikeSigma * running sigma
    double  rate    = 0.0;      // reported sample rate
    qint64  lastT   = 0;        // last sample timestamp
    double  latencySum = 0.0;   // now - sample time, averaged
    quint64 latencyN   = 0;
    double  maxLatency = 0.0;
    // running mean/variance of the vector magnitude, for spike detection
    double  mean = 0.0, m2 = 0.0;
    quint64 n = 0;
};

class QcApplication : public tp::client::Application {
public:
    QcApplication(tp::client::ApplicationSettings settings, int periodMs, double spikeSigma)
        : Application(std::move(settings)), m_periodMs(periodMs), m_spikeSigma(spikeSigma) {}

    bool init() override {
        if (!Application::init()) return false;
        // Reporting cadence is this module's own concern; the base class only
        // owns the message pump and the heartbeat.
        QObject::connect(&m_reportTimer, &QTimer::timeout, [this]() { report(); });
        m_reportTimer.start(m_periodMs);

        std::printf("[tpqc] raw <- %s   qc -> broker   period=%ds\n",
                    messagingUrl().toUtf8().constData(), m_periodMs / 1000);
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["published"] = static_cast<qulonglong>(m_published);
        c["sensors"]   = m_stats.size();
        return c;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (!topic.startsWith("raw.")) return;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        const quint32 obj = h.value("object").toUInt();
        const quint32 sen = h.value("sensor").toUInt();
        Qc& q = m_stats[(quint64(obj) << 20) | sen];

        q.station = h.value("station").toUInt();
        const double rate = h.value("sampleRate").toDouble();
        if (rate > 0) q.rate = rate;

        tp::messaging::forEachSample(h, [&](double x, double y, double z, qint64 t, int) {
            // Gap: a jump larger than 1.5 sample periods (allows normal jitter).
            if (q.lastT > 0 && q.rate > 0) {
                const double dt = 1000.0 / q.rate;
                if (double(t - q.lastT) > dt * 1.5) ++q.gaps;
            }
            q.lastT = t;
            ++q.samples;

            // Latency: how far behind real time the data arrives.
            const double lat = double(now - t);
            q.latencySum += lat; ++q.latencyN;
            q.maxLatency = std::max(q.maxLatency, lat);

            // Spike: vector magnitude far outside the running distribution
            // (Welford running mean/variance).
            const double mag = std::sqrt(x*x + y*y + z*z);
            ++q.n;
            const double d = mag - q.mean;
            q.mean += d / double(q.n);
            q.m2   += d * (mag - q.mean);
            if (q.n > 50) {
                const double sigma = std::sqrt(q.m2 / double(q.n - 1));
                if (sigma > 1e-9 && std::fabs(mag - q.mean) > m_spikeSigma * sigma) ++q.spikes;
            }
        });
    }

private:
    void report() {
        const double periodSec = m_periodMs / 1000.0;
        for (auto it = m_stats.begin(); it != m_stats.end(); ++it) {
            Qc& q = it.value();
            const quint32 obj = quint32(it.key() >> 20);
            const quint32 sen = quint32(it.key() & 0xFFFFF);

            const double expected = q.rate > 0 ? q.rate * periodSec : 0.0;
            const double availability = expected > 0
                ? std::min(100.0, 100.0 * double(q.samples) / expected) : 0.0;
            const double avgLatency = q.latencyN ? q.latencySum / double(q.latencyN) : 0.0;

            QVariantMap out;
            out["v"] = 1; out["type"] = "qc";
            out["stationId"] = q.station; out["objectId"] = obj; out["sensorId"] = sen;
            out["timestamp"]    = static_cast<qlonglong>(QDateTime::currentMSecsSinceEpoch());
            out["samples"]      = static_cast<qulonglong>(q.samples);
            out["expected"]     = expected;
            out["availability"] = availability;
            out["gaps"]         = static_cast<qulonglong>(q.gaps);
            out["spikes"]       = static_cast<qulonglong>(q.spikes);
            out["sampleRate"]   = q.rate;
            out["latencyMs"]    = avgLatency;
            out["maxLatencyMs"] = q.maxLatency;
            // Simple verdict an operator can act on.
            const char* verdict = availability >= 95.0 && q.gaps == 0 ? "GOOD"
                                : availability >= 80.0 ? "DEGRADED" : "BAD";
            out["verdict"] = verdict;

            publish(tp::messaging::make("qc", q.station, obj, sen, out));
            ++m_published;

            std::printf("[tpqc] obj%u/sen%u %-8s avail=%.1f%% gaps=%llu spikes=%llu "
                        "lat=%.0fms (max %.0f) rate=%.0fHz\n",
                        obj, sen, verdict, availability,
                        static_cast<unsigned long long>(q.gaps),
                        static_cast<unsigned long long>(q.spikes),
                        avgLatency, q.maxLatency, q.rate);

            // Reset the per-period counters (running spike stats persist).
            q.samples = 0; q.gaps = 0; q.spikes = 0;
            q.latencySum = 0; q.latencyN = 0; q.maxLatency = 0;
        }
        std::fflush(stdout);
    }

    QHash<quint64, Qc> m_stats;      // key = obj<<20 | sen
    quint64 m_published = 0;
    QTimer  m_reportTimer;
    int     m_periodMs;
    double  m_spikeSigma;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpqc");

    // Configuration is read by the platform; the CLI still overrides it.
    tp::Config cfg;
    cfg.load("tpqc");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse data-quality monitor");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption queueOpt("queue", "Queue: production | playback", "name",
                                cfg.str("connection.queue", "production"));
    QCommandLineOption perOpt("period-sec", "Reporting period (s)", "s",
                              QString::number(cfg.integer("qc.periodSec", 5)));
    parser.addOptions({masterOpt, queueOpt, perOpt});
    parser.process(app);

    tp::client::ApplicationSettings settings;
    settings.moduleName = "tpqc";
    settings.masterHost = parser.value(masterOpt);
    settings.queue      = parser.value(queueOpt);
    settings.subscriptions = {"raw."};
    settings.sohIntervalSeconds = 2;

    QcApplication qc(std::move(settings),
                     std::max(1000, parser.value(perOpt).toInt() * 1000),
                     cfg.number("qc.spikeSigma", 8.0));
    return qc.exec();
}
