// tpimport — replay archived miniSEED waveforms back into the system (like
// SeisComp's msrtsimul). Reads a sensor's 3 channels from a TDS archive, re-
// interleaves X/Y/Z by time, and publishes raw samples to a queue (usually
// playback) so tpproc/console can re-analyse or review recorded data.
//
//   tpimport --tds var/tds --object 1 --sensor 1 --queue playback [--historic] [--speed 1]

#include "terrapulse/client/application.h"
#include "mseed/Mseed.h"
#include "mseed/RecordStream.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

// A source module: publisher-only. init() loads the archive, then (after the
// slow-joiner settle) a timer paces the samples out and quits when done.
class ImportApplication : public tp::client::Application {
public:
    ImportApplication(tp::client::ApplicationSettings settings, std::string dir,
                      quint32 station, quint32 object, quint32 sensor,
                      bool historic, double speed)
        : Application(std::move(settings)), m_dir(std::move(dir)),
          m_station(station), m_object(object), m_sensor(sensor),
          m_historic(historic), m_speed(speed),
          m_topic("raw." + std::to_string(station) + "." + std::to_string(object)
                        + "." + std::to_string(sensor)) {}

    bool init() override {
        m_x = tp::mseed::readTdsChannel(m_dir, tp::mseed::sourceId(m_object, m_sensor, 0));
        m_y = tp::mseed::readTdsChannel(m_dir, tp::mseed::sourceId(m_object, m_sensor, 1));
        m_z = tp::mseed::readTdsChannel(m_dir, tp::mseed::sourceId(m_object, m_sensor, 2));
        m_n = std::min({ m_x.samples.size(), m_y.samples.size(), m_z.samples.size() });
        m_rate = m_x.sampleRate > 0 ? m_x.sampleRate : 200.0;
        if (m_n == 0) {
            std::fprintf(stderr, "tpimport: no samples found for object %u sensor %u in %s\n",
                         m_object, m_sensor, m_dir.c_str());
            return false;
        }

        if (!Application::init()) return false;

        std::printf("[tpimport] %zu samples/axis @ %.0f Hz  ->  %s  via %s (%s, x%.2g)\n",
                    m_n, m_rate, m_topic.c_str(), messagingUrl().toUtf8().constData(),
                    m_historic ? "historic" : "realtime", m_speed);
        std::fflush(stdout);

        m_perTick = std::max(1, int(std::llround(m_rate * m_speed * kTickMs / 1000.0)));
        m_nowBase = QDateTime::currentMSecsSinceEpoch();
        QObject::connect(&m_timer, &QTimer::timeout, [this]() { tick(); });
        // Delay so subscriptions propagate (PUB/SUB slow-joiner).
        QTimer::singleShot(1300, [this]() { m_timer.start(kTickMs); });
        return true;
    }

private:
    void tick() {
        for (int i = 0; i < m_perTick && m_idx < m_n; ++i, ++m_idx) {
            const int64_t base = m_historic ? m_x.startTimeMs : m_nowBase;
            const int64_t t    = base + int64_t(std::llround(m_idx * 1000.0 / m_rate));

            QVariantMap h;
            h["v"] = 1; h["type"] = "raw";
            h["station"] = m_station; h["object"] = m_object; h["sensor"] = m_sensor;
            h["t"] = static_cast<qlonglong>(t);
            h["x"] = m_x.samples[m_idx] / 10000.0;
            h["y"] = m_y.samples[m_idx] / 10000.0;
            h["z"] = m_z.samples[m_idx] / 10000.0;
            h["seq"] = static_cast<qulonglong>(++m_seq);
            h["sampleRate"] = m_rate;
            publish(m_topic, h);
        }
        if (m_idx >= m_n) {
            std::printf("[tpimport] replay done (%zu samples)\n", m_n);
            std::fflush(stdout);
            m_timer.stop();
            QTimer::singleShot(400, [this]() { exit(0); });
        }
    }

    static constexpr int kTickMs = 10;
    std::string m_dir;
    quint32 m_station, m_object, m_sensor;
    bool    m_historic;
    double  m_speed;
    std::string m_topic;

    tp::mseed::Trace m_x, m_y, m_z;
    std::size_t m_n = 0, m_idx = 0;
    double  m_rate = 200.0;
    int     m_perTick = 1;
    quint64 m_seq = 0;
    int64_t m_nowBase = 0;
    QTimer  m_timer;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpimport");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse miniSEED replay");
    parser.addHelpOption();
    QCommandLineOption tdsOpt    ("tds",    "TDS archive directory", "dir");
    QCommandLineOption masterOpt ({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    QCommandLineOption queueOpt  ("queue",  "Queue: production | playback", "name", "playback");
    QCommandLineOption stationOpt("station", "Station id", "id", "1");
    QCommandLineOption objectOpt ("object",  "Object id",  "id", "1");
    QCommandLineOption sensorOpt ("sensor",  "Sensor id",  "id", "1");
    QCommandLineOption historicOpt("historic", "Keep original timestamps (default: retime to now)");
    QCommandLineOption speedOpt  ("speed", "Replay speed factor", "x", "1");
    parser.addOptions({tdsOpt, masterOpt, queueOpt, stationOpt, objectOpt, sensorOpt, historicOpt, speedOpt});
    parser.process(app);

    if (!parser.isSet(tdsOpt)) { std::fprintf(stderr, "tpimport: --tds <dir> is required\n"); return 2; }

    tp::client::ApplicationSettings settings;
    settings.moduleName = "tpimport";
    settings.masterHost = parser.value(masterOpt);
    settings.queue      = parser.value(queueOpt);
    settings.sohIntervalSeconds = 0;        // short-lived source: no heartbeat

    ImportApplication imp(std::move(settings), parser.value(tdsOpt).toStdString(),
                          parser.value(stationOpt).toUInt(),
                          parser.value(objectOpt).toUInt(),
                          parser.value(sensorOpt).toUInt(),
                          parser.isSet(historicOpt),
                          std::max(0.01, parser.value(speedOpt).toDouble()));
    return imp.exec();
}
