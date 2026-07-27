// tpproc — TerraPulse processing daemon.
// Subscribes to raw samples, runs the analysis + history pipeline, and publishes
// SAF/SHF results on the bus. No GUI. This is the standalone replacement for the
// analysis that used to live inside the Qt app.

#include "proc/ProcPipeline.h"
#include "terrapulse/client/application.h"
#include "terrapulse/messaging/rawsamples.h"
#include "slink/WaveformClient.h"
#include <memory>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QSet>
#include <QPair>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <string>

namespace {

std::string topicFor(const char* kind, const QVariantMap& m, bool withAxis) {
    std::string t = std::string(kind) + "."
        + std::to_string(m.value("stationId").toUInt()) + "."
        + std::to_string(m.value("objectId").toUInt())  + "."
        + std::to_string(m.value("sensorId").toUInt());
    if (withAxis)
        t += "." + std::to_string(m.value("component").toInt());
    return t;
}

class ProcApplication : public tp::client::Application {
public:
    ProcApplication(tp::client::ApplicationSettings settings,
                    const tp::AnalysisThresholds& thresholds, tp::Config cfg,
                    int window, int freqWindow, QString slinkHost = {}, quint16 slinkPort = 0)
        : Application(std::move(settings)), m_thresholds(thresholds),
          m_cfg(std::move(cfg)), m_window(window), m_freqWindow(freqWindow),
          m_slinkHost(std::move(slinkHost)), m_slinkPort(slinkPort) {}

    bool init() override {
        if (!Application::init()) return false;

        m_pipe.setWindowSize(m_window);
        m_pipe.setFreqWindow(m_freqWindow);
        m_pipe.setCalibrationWindows(m_cfg.integer("proc.calibrationWindows", 20));
        m_pipe.setFreqBand(m_cfg.number("proc.freqBandMin", 0.3), m_cfg.number("proc.freqBandMax", 0.0));
        m_pipe.setStaLta(m_cfg.number("proc.staLta.staAlpha", 0.34),
                         m_cfg.number("proc.staLta.ltaAlpha", 0.02),
                         m_cfg.number("proc.staLta.onRatio",  3.0),
                         m_cfg.number("proc.staLta.offRatio", 1.5));
        m_pipe.setThresholds(m_thresholds);

        m_pipe.onSaf = [this](const QVariantMap& m) {
            if (m.value("triggered").toBool()) ++m_trigWindows;
            m_maxRatio = std::max(m_maxRatio, m.value("staLtaRatio").toDouble());
            if (m.value("component").toInt() == 0)      // X axis carries the structural tone
                m_lastFreqX = m.value("dominantFrequency").toDouble();
            publish(tp::messaging::make(topicFor("saf", m, /*withAxis=*/true), m));
            ++m_safOut;
        };
        m_pipe.onShf = [this](const QVariantMap& m) {
            publish(tp::messaging::make(topicFor("shf", m, /*withAxis=*/false), m));
            ++m_shfOut;
        };

        // Level 2: read waveforms from the SeedLink backbone instead of the bus.
        if (m_slinkPort) {
            m_wf = std::make_unique<tp::slink::WaveformClient>(m_slinkHost, m_slinkPort);
            m_wf->onTriple([this](uint32_t sta, uint32_t obj, uint32_t sen,
                                  double x, double y, double z, int64_t t, double rate) {
                feedTriple(sta, obj, sen, x, y, z, t, quint32(rate));
            });
            m_wf->start();
        }

        std::printf("[tpproc] waveforms <- %s   saf/shf -> broker   window=%d\n",
                    m_slinkPort ? (m_slinkHost + ":" + QString::number(m_slinkPort)).toUtf8().constData()
                                : messagingUrl().toUtf8().constData(),
                    m_pipe.windowSize());
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["rawIn"]   = static_cast<qulonglong>(m_rawIn);
        c["windows"] = m_pipe.windowsProcessed();
        c["saf"]     = static_cast<qulonglong>(m_safOut);
        c["shf"]     = static_cast<qulonglong>(m_shfOut);
        return c;
    }

    // One triaxial sample into the pipeline — shared by the bus path and the
    // SeedLink WaveformClient path (Level 2). Resolves the sensor's binding
    // profile on first sight.
    void feedTriple(quint32 sta, quint32 obj, quint32 sen,
                    double x, double y, double z, qint64 t, quint32 rate) {
        applyBinding(obj, sen);
        m_pipe.addSample(sta, obj, sen, x, y, z, t, rate);
        ++m_rawIn;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (!topic.startsWith("raw.")) return;
        const quint32 hObj = h.value("object").toUInt();
        const quint32 hSen = h.value("sensor").toUInt();
        const quint32 sta  = h.value("station").toUInt();
        const quint32 rate = h.value("sampleRate").toUInt();
        tp::messaging::forEachSample(h, [&](double x, double y, double z, qint64 t, int) {
            feedTriple(sta, hObj, hSen, x, y, z, t, rate);
        });
    }

    // First time we see a sensor, apply its binding profile (if any) so this
    // structure is judged by its own thresholds.
    void applyBinding(quint32 hObj, quint32 hSen) {
        if (!m_pipe.hasThresholdsFor(hObj, hSen) && !m_bound.contains(qMakePair(hObj, hSen))) {
            m_bound.insert(qMakePair(hObj, hSen));
            tp::Config bcfg;
            bcfg.load("tpproc");
            const QString profile = bcfg.loadBinding("tpproc", hObj, hSen);
            if (!profile.isEmpty()) {
                tp::AnalysisThresholds bt = m_thresholds;
                bt.rmsWarningFactor  = float(bcfg.number("proc.threshold.rmsWarning",  bt.rmsWarningFactor));
                bt.rmsCriticalFactor = float(bcfg.number("proc.threshold.rmsCritical", bt.rmsCriticalFactor));
                bt.ampWarning        = float(bcfg.number("proc.threshold.ampWarning",  bt.ampWarning));
                bt.ampCritical       = float(bcfg.number("proc.threshold.ampCritical", bt.ampCritical));
                bt.freqShiftWarning  = float(bcfg.number("proc.threshold.freqShiftWarning",  bt.freqShiftWarning));
                bt.freqShiftCritical = float(bcfg.number("proc.threshold.freqShiftCritical", bt.freqShiftCritical));
                m_pipe.setThresholdsFor(hObj, hSen, bt);
                std::printf("[tpproc] binding: object %u sensor %u -> profile '%s'\n",
                            hObj, hSen, profile.toUtf8().constData());
                std::fflush(stdout);
            }
        }
    }

    void handleSOH() override {
        std::printf("[tpproc] rawIn=%llu windows=%d saf=%llu shf=%llu trig=%llu maxRatio=%.2f fX=%.2fHz\n",
                    static_cast<unsigned long long>(m_rawIn),
                    m_pipe.windowsProcessed(),
                    static_cast<unsigned long long>(m_safOut),
                    static_cast<unsigned long long>(m_shfOut),
                    static_cast<unsigned long long>(m_trigWindows),
                    m_maxRatio, m_lastFreqX);
        std::fflush(stdout);
    }

private:
    tp::ProcPipeline m_pipe;
    tp::AnalysisThresholds m_thresholds;
    tp::Config m_cfg;
    int m_window, m_freqWindow;

    QSet<QPair<quint32,quint32>> m_bound;   // sensors whose binding we resolved
    quint64 m_rawIn = 0, m_safOut = 0, m_shfOut = 0, m_trigWindows = 0;
    double  m_maxRatio = 0.0, m_lastFreqX = 0.0;
    QString m_slinkHost;
    quint16 m_slinkPort = 0;
    std::unique_ptr<tp::slink::WaveformClient> m_wf;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpproc");

    // Defaults come from the layered configuration; a command-line flag, if given,
    // overrides the file value (etc/defaults -> etc -> ~/.terrapulse -> CLI).
    tp::Config cfg;
    cfg.load("tpproc");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse processing daemon");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption queueOpt("queue", "Queue: production | playback", "name",
                                cfg.str("connection.queue", "production"));
    QCommandLineOption winOpt("window", "Samples per analysis window", "n",
                              QString::number(cfg.integer("proc.window", 100)));
    QCommandLineOption freqWinOpt("freq-window",
        "Samples used for the natural-frequency estimate (longer = finer resolution)", "n",
        QString::number(cfg.integer("proc.freqWindow", 1024)));
    QCommandLineOption slinkOpt("slink", "Read waveforms from a SeedLink backbone host:port "
                                "instead of the raw. bus (Level 2)", "host:port", "");
    parser.addOptions({masterOpt, queueOpt, winOpt, freqWinOpt, slinkOpt});
    parser.process(app);

    QString slinkHost; quint16 slinkPort = 0;
    if (const QString sl = parser.value(slinkOpt); !sl.isEmpty()) {
        const int colon = sl.lastIndexOf(':');
        slinkHost = colon > 0 ? sl.left(colon) : sl;
        slinkPort = quint16(colon > 0 ? sl.mid(colon + 1).toUInt() : 18000);
    }

    // Detection thresholds and health weights — the engineer-tunable knobs.
    tp::AnalysisThresholds th;
    th.rmsWarningFactor    = float(cfg.number("proc.threshold.rmsWarning",     th.rmsWarningFactor));
    th.rmsCriticalFactor   = float(cfg.number("proc.threshold.rmsCritical",    th.rmsCriticalFactor));
    th.energyWarningFactor = float(cfg.number("proc.threshold.energyWarning",  th.energyWarningFactor));
    th.energyCriticalFactor= float(cfg.number("proc.threshold.energyCritical", th.energyCriticalFactor));
    th.ampWarning          = float(cfg.number("proc.threshold.ampWarning",     th.ampWarning));
    th.ampCritical         = float(cfg.number("proc.threshold.ampCritical",    th.ampCritical));
    th.freqShiftWarning    = float(cfg.number("proc.threshold.freqShiftWarning",  th.freqShiftWarning));
    th.freqShiftCritical   = float(cfg.number("proc.threshold.freqShiftCritical", th.freqShiftCritical));
    th.healthWarning       = float(cfg.number("proc.threshold.healthWarning",  th.healthWarning));
    th.healthCritical      = float(cfg.number("proc.threshold.healthCritical", th.healthCritical));
    th.wRms                = float(cfg.number("proc.health.wRms",    th.wRms));
    th.wEnergy             = float(cfg.number("proc.health.wEnergy", th.wEnergy));
    th.wAmp                = float(cfg.number("proc.health.wAmp",    th.wAmp));
    th.wFreq               = float(cfg.number("proc.health.wFreq",   th.wFreq));

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpproc";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = parser.value(queueOpt);
    settings.subscriptions = slinkPort ? QStringList{} : QStringList{"raw."};  // backbone vs bus
    settings.sohIntervalSeconds = 2;
    settings.pollIntervalMs = 10;   // raw rate: keep latency low

    ProcApplication proc(std::move(settings), th, cfg,
                         parser.value(winOpt).toInt(),
                         parser.value(freqWinOpt).toInt(),
                         slinkHost, slinkPort);
    return proc.exec();
}
