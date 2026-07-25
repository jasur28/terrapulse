// tpwfparam — strong-motion parameters (SeisComp scwfparam analog). Subscribes to
// raw acceleration, keeps a rolling window per sensor, and periodically computes
// PGA, PGV, a 5%-damped response spectrum (peak PSA + period), and JMA seismic
// intensity (計測震度). Publishes `wfp.` parameter messages — the structural
// "magnitude": how strongly a structure was shaken, its resonant response, and an
// intensity score an operator can read at a glance. No GUI.
//
//   tpwfparam [--master host] [--queue name] [--window-sec 10] [--period-sec 2]

#include "analysis/StrongMotion.h"
#include "terrapulse/client/application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

namespace {

struct Buf {
    std::deque<float> x, y, z;
    double  rate = 200.0;
    quint32 station = 1;
    qint64  lastT = 0;
};

class WfParamApplication : public tp::client::Application {
public:
    WfParamApplication(tp::client::ApplicationSettings settings, double windowSec, int periodMs,
                       std::vector<double> periods, double damping)
        : Application(std::move(settings)), m_windowSec(windowSec), m_periodMs(periodMs),
          m_periods(std::move(periods)), m_damping(damping) {}

    bool init() override {
        if (!Application::init()) return false;
        QObject::connect(&m_computeTimer, &QTimer::timeout, [this]() { compute(); });
        m_computeTimer.start(m_periodMs);

        std::printf("[tpwfparam] raw <- %s   wfp -> broker   window=%.0fs period=%dms\n",
                    messagingUrl().toUtf8().constData(), m_windowSec, m_periodMs);
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["published"] = static_cast<qulonglong>(m_published);
        c["sensors"]   = m_bufs.size();
        return c;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (!topic.startsWith("raw.")) return;
        const quint32 obj = h.value("object").toUInt();
        const quint32 sen = h.value("sensor").toUInt();
        const quint64 key = (quint64(obj) << 20) | sen;
        Buf& b = m_bufs[key];
        b.station = h.value("station").toUInt();
        b.rate    = h.value("sampleRate").toDouble() > 0 ? h.value("sampleRate").toDouble() : 200.0;
        b.lastT   = h.value("t").toLongLong();
        b.x.push_back(float(h.value("x").toDouble()));
        b.y.push_back(float(h.value("y").toDouble()));
        b.z.push_back(float(h.value("z").toDouble()));
        const std::size_t cap = std::size_t(b.rate * m_windowSec);
        while (b.x.size() > cap) { b.x.pop_front(); b.y.pop_front(); b.z.pop_front(); }
    }

    void handleSOH() override {
        std::printf("[tpwfparam] published=%llu sensors=%d  last: PGA=%.1f gal PGV=%.2f cm/s "
                    "PSA=%.1f gal JMA=%.2f (%s)\n",
                    static_cast<unsigned long long>(m_published), m_bufs.size(),
                    m_lastPga, m_lastPgv, m_lastPsa, m_lastJma,
                    m_lastScale.toUtf8().constData());
        std::fflush(stdout);
    }

private:
    void compute() {
        for (auto it = m_bufs.begin(); it != m_bufs.end(); ++it) {
            Buf& b = it.value();
            if (b.x.size() < std::size_t(b.rate)) continue;   // need >= 1 s
            const double dt = 1.0 / b.rate;
            const std::vector<float> xv(b.x.begin(), b.x.end());
            const std::vector<float> yv(b.y.begin(), b.y.end());
            const std::vector<float> zv(b.z.begin(), b.z.end());

            const auto dx = tp::sm::detrend(xv);
            const auto dy = tp::sm::detrend(yv);
            const auto dz = tp::sm::detrend(zv);

            // Vector PGA (gravity removed by detrend); PGV = max horizontal.
            double pgaVec = 0.0;
            for (std::size_t i = 0; i < dx.size(); ++i)
                pgaVec = std::max(pgaVec, std::sqrt(dx[i]*dx[i] + dy[i]*dy[i] + dz[i]*dz[i]));
            const double pgv = std::max(tp::sm::pgv(dx, dt), tp::sm::pgv(dy, dt));

            // Response spectrum: keep the envelope over all axes, not just the peak,
            // so the console can plot it against a design code.
            std::vector<double> psaEnv(m_periods.size(), 0.0);
            double psaMax = 0.0, psaPeriod = 0.0;
            for (const auto* ax : { &dx, &dy, &dz }) {
                const auto psa = tp::sm::responseSpectrum(*ax, dt, m_periods, m_damping);
                for (std::size_t p = 0; p < psa.size(); ++p) {
                    psaEnv[p] = std::max(psaEnv[p], psa[p]);
                    if (psa[p] > psaMax) { psaMax = psa[p]; psaPeriod = m_periods[p]; }
                }
            }
            QVariantList psaList, periodList;
            for (std::size_t p = 0; p < m_periods.size(); ++p) {
                psaList.append(psaEnv[p]);
                periodList.append(m_periods[p]);
            }

            const double jma = tp::sm::jmaIntensity(xv, yv, zv, dt);
            m_lastPga = pgaVec; m_lastPgv = pgv; m_lastPsa = psaMax;
            m_lastJma = jma; m_lastScale = tp::sm::jmaScale(jma);

            const quint32 obj = quint32(it.key() >> 20);
            const quint32 sen = quint32(it.key() & 0xFFFFF);
            QVariantMap out;
            out["v"] = 1; out["type"] = "wfparam";
            out["stationId"] = b.station; out["objectId"] = obj; out["sensorId"] = sen;
            out["timestamp"] = static_cast<qlonglong>(b.lastT);
            out["pga"] = pgaVec; out["pgv"] = pgv;
            out["psaMax"] = psaMax; out["psaPeriod"] = psaPeriod;
            out["psa"] = psaList; out["periods"] = periodList;   // full spectrum to plot
            out["jma"] = jma; out["jmaScale"] = tp::sm::jmaScale(jma);

            publish(tp::messaging::make("wfp", b.station, obj, sen, out));
            ++m_published;
        }
    }

    QHash<quint64, Buf> m_bufs;      // key = obj<<20 | sen
    quint64 m_published = 0;
    double  m_lastPga = 0, m_lastPgv = 0, m_lastPsa = 0, m_lastJma = 0;
    QString m_lastScale = "0";
    QTimer  m_computeTimer;
    double  m_windowSec;
    int     m_periodMs;
    std::vector<double> m_periods;
    double  m_damping;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpwfparam");

    tp::Config cfg;                       // file defaults; CLI flags override
    cfg.load("tpwfparam");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse strong-motion parameters");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption queueOpt("queue", "Queue: production | playback", "name",
                                cfg.str("connection.queue", "production"));
    QCommandLineOption winOpt("window-sec", "Rolling window length (s)", "s",
                              QString::number(cfg.integer("wfparam.windowSec", 10)));
    QCommandLineOption perOpt("period-sec", "Compute period (s)", "s",
                              QString::number(cfg.integer("wfparam.periodSec", 2)));
    parser.addOptions({masterOpt, queueOpt, winOpt, perOpt});
    parser.process(app);

    // Response-spectrum periods and damping come from configuration so an
    // engineer can match them to the monitored structures' natural periods.
    std::vector<double> periods;
    for (const QString& p : cfg.str("wfparam.periods", "0.1,0.15,0.2,0.3,0.5,0.75,1.0,1.5,2.0,3.0")
                               .split(',', Qt::SkipEmptyParts)) {
        bool ok = false; const double v = p.trimmed().toDouble(&ok);
        if (ok && v > 0) periods.push_back(v);
    }
    if (periods.empty()) periods = {0.1,0.2,0.5,1.0,2.0};

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpwfparam";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = parser.value(queueOpt);
    settings.subscriptions = {"raw."};
    settings.sohIntervalSeconds = 2;
    settings.pollIntervalMs = 20;   // raw rate: keep latency low

    WfParamApplication wf(std::move(settings),
                          qMax(2, parser.value(winOpt).toInt()),
                          qMax(500, parser.value(perOpt).toInt() * 1000),
                          std::move(periods),
                          cfg.number("wfparam.damping", 0.05));
    return wf.exec();
}
