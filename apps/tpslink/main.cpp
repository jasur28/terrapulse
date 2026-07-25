// tpslink — SeedLink acquisition client. Streams miniSEED from any SeedLink
// server (third-party dataloggers / vendor devices) and feeds it into
// TerraPulse: publishes triaxial raw.<sta>.<obj>.<sen> samples on the bus and
// (optionally) archives the waveforms as miniSEED. This is how the system
// interoperates with instruments we didn't build, alongside our own STM32 via
// tpacq. SeedLink is a public protocol — implemented here from its spec, not
// from any GPL server source.
//
//   tpslink --server geofon.gfz.de:18000 --net GE --sta MORC --select "HH?" \
//           --object 1 --sensor 1 [--archive var/tds] [--queue production]
//
// Channel orientation is taken from the last character of the SEED channel code
// (Z/N/E, X/Y/Z or 1/2/3) and mapped to our component axes X=0, Y=1, Z=2. The
// three channels are re-interleaved by sample time into triaxial raw messages.

#include "terrapulse/client/application.h"
#include "mseed/Mseed.h"
#include "mseed/TdsArchive.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTcpSocket>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace {

// SEED channel code -> our component (X=0, Y=1, Z=2). Returns -1 if unknown.
int componentOf(const std::string& sid) {
    if (sid.empty()) return -1;
    switch (sid.back()) {
        case 'Z': case 'z': case '3':            return 2;
        case 'N': case 'n': case 'Y': case 'y': case '2': return 1;
        case 'E': case 'e': case 'X': case 'x': case '1': return 0;
        default:                                  return -1;
    }
}

// Send a SeedLink command and wait for the server's line(s). Returns the reply.
QByteArray command(QTcpSocket& s, const QByteArray& cmd, int waitMs = 3000) {
    if (!cmd.isEmpty()) { s.write(cmd + "\r\n"); s.flush(); }
    s.waitForReadyRead(waitMs);
    return s.readAll();
}

struct Options {
    QString host; quint16 port = 18000;
    QString net, sta, select;
    bool    hasSelect = false;
    quint32 station = 1, object = 1, sensor = 1;
    QString archiveDir;
    bool    archive = false;
};

class SlinkApplication : public tp::client::Application {
public:
    SlinkApplication(tp::client::ApplicationSettings settings, Options opt)
        : Application(std::move(settings)), m_opt(std::move(opt)),
          m_topic("raw." + std::to_string(m_opt.station) + "." + std::to_string(m_opt.object)
                        + "." + std::to_string(m_opt.sensor)) {}

    bool init() override {
        if (!Application::init()) return false;    // publisher ready before streaming

        if (m_opt.archive)
            m_tds = std::make_unique<tp::mseed::TdsArchive>(m_opt.archiveDir.toStdString());

        // ── Connect + SeedLink handshake (synchronous) ────────────────────────
        m_sock.connectToHost(m_opt.host, m_opt.port);
        if (!m_sock.waitForConnected(5000)) {
            std::fprintf(stderr, "tpslink: cannot connect to %s:%u: %s\n",
                         m_opt.host.toUtf8().constData(), m_opt.port,
                         m_sock.errorString().toUtf8().constData());
            return false;
        }

        const QByteArray hello = command(m_sock, "HELLO");
        std::printf("[tpslink] %s:%u  %s\n", m_opt.host.toUtf8().constData(), m_opt.port,
                    hello.trimmed().replace('\r', ' ').replace('\n', ' ').constData());

        // Uni-station handshake: STATION -> SELECT (optional) -> DATA -> END.
        const QByteArray staCmd = "STATION " + m_opt.sta.toUtf8() + " " + m_opt.net.toUtf8();
        if (!command(m_sock, staCmd).startsWith("OK")) {
            std::fprintf(stderr, "tpslink: STATION rejected by server\n"); return false;
        }
        if (m_opt.hasSelect && !command(m_sock, "SELECT " + m_opt.select.toUtf8()).startsWith("OK"))
            std::fprintf(stderr, "tpslink: SELECT rejected (continuing with all channels)\n");
        if (!command(m_sock, "DATA").startsWith("OK")) {
            std::fprintf(stderr, "tpslink: DATA rejected by server\n"); return false;
        }
        m_sock.write("END\r\n"); m_sock.flush();   // begin streaming

        QObject::connect(&m_sock, &QTcpSocket::readyRead, [this]() { onReadyRead(); });

        std::printf("[tpslink] streaming %s.%s -> %s  via %s%s\n",
                    m_opt.net.toUtf8().constData(), m_opt.sta.toUtf8().constData(),
                    m_topic.c_str(), messagingUrl().toUtf8().constData(),
                    m_tds ? "  (+archive)" : "");
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["records"]   = static_cast<qulonglong>(m_records);
        c["published"] = static_cast<qulonglong>(m_published);
        c["src"]       = "seedlink";
        return c;
    }

protected:
    void handleSOH() override {
        if (m_tds) m_tds->flushAll();
        std::printf("[tpslink] records=%llu published=%llu\n",
                    static_cast<unsigned long long>(m_records),
                    static_cast<unsigned long long>(m_published));
        std::fflush(stdout);
    }

private:
    // Re-interleave the three per-component queues into triaxial raw messages.
    void emitTriples() {
        auto& X = m_axis[0]; auto& Y = m_axis[1]; auto& Z = m_axis[2];
        while (!X.empty() && !Y.empty() && !Z.empty()) {
            const int64_t tx = X.front().first, ty = Y.front().first, tz = Z.front().first;
            const int64_t tmin = std::min({tx, ty, tz}), tmax = std::max({tx, ty, tz});
            const double tol = m_lastRate > 0 ? 1000.0 / m_lastRate / 2 : 5.0;
            if (double(tmax - tmin) > tol) {              // resync: drop the laggards
                if (tx < tmax - tol) X.pop_front();
                if (ty < tmax - tol) Y.pop_front();
                if (tz < tmax - tol) Z.pop_front();
                continue;
            }
            QVariantMap h;
            h["v"] = 1; h["type"] = "raw";
            h["station"] = m_opt.station; h["object"] = m_opt.object; h["sensor"] = m_opt.sensor;
            h["t"] = static_cast<qlonglong>(tx);
            h["x"] = X.front().second / 10000.0;
            h["y"] = Y.front().second / 10000.0;
            h["z"] = Z.front().second / 10000.0;
            h["seq"] = static_cast<qulonglong>(++m_published);
            h["sampleRate"] = m_lastRate;
            publish(m_topic, h);
            X.pop_front(); Y.pop_front(); Z.pop_front();
        }
    }

    // Parse 520-byte SeedLink packets (8-byte "SL" header + 512-byte miniSEED).
    void onReadyRead() {
        m_buf += m_sock.readAll();
        while (m_buf.size() >= 520) {
            if (m_buf[0] != 'S' || m_buf[1] != 'L') { m_buf.remove(0, 1); continue; }  // resync
            const QByteArray rec = m_buf.mid(8, 512);
            m_buf.remove(0, 520);
            for (auto& d : tp::mseed::decode(rec.constData(), rec.size())) {
                ++m_records;
                if (d.sampleRate > 0) m_lastRate = d.sampleRate;
                const int comp = componentOf(d.sid);
                if (comp < 0) continue;
                if (m_tds)
                    for (std::size_t i = 0; i < d.samples.size(); ++i)
                        m_tds->addSample(m_opt.object, m_opt.sensor, comp, d.samples[i],
                                         d.startTimeMs + int64_t(i * 1000.0 / d.sampleRate), d.sampleRate);
                for (std::size_t i = 0; i < d.samples.size(); ++i)
                    m_axis[comp].emplace_back(d.startTimeMs + int64_t(i * 1000.0 / d.sampleRate), d.samples[i]);
            }
            emitTriples();
        }
    }

    Options m_opt;
    std::string m_topic;
    QTcpSocket m_sock;
    QByteArray m_buf;
    std::map<int, std::deque<std::pair<int64_t, int32_t>>> m_axis;   // comp -> (timeMs, count)
    std::unique_ptr<tp::mseed::TdsArchive> m_tds;
    quint64 m_published = 0, m_records = 0;
    double  m_lastRate = 200.0;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpslink");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse SeedLink acquisition client");
    parser.addHelpOption();
    QCommandLineOption serverOpt ("server",  "SeedLink server host[:port] (default port 18000)", "host");
    QCommandLineOption netOpt    ("net",     "Network code",  "NN", "XX");
    QCommandLineOption staOpt    ("sta",     "Station code",  "SSSS");
    QCommandLineOption selOpt    ("select",  "SELECT pattern (e.g. HN? or 00HNZ)", "pat");
    QCommandLineOption masterOpt ({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    QCommandLineOption queueOpt  ("queue",   "Target queue: production | playback", "name", "production");
    QCommandLineOption archiveOpt("archive", "Also archive raw waveforms as miniSEED into this TDS dir", "dir");
    QCommandLineOption stationOpt("station", "TerraPulse station id", "id", "1");
    QCommandLineOption objectOpt ("object",  "TerraPulse object id",  "id", "1");
    QCommandLineOption sensorOpt ("sensor",  "TerraPulse sensor id",  "id", "1");
    parser.addOptions({serverOpt, netOpt, staOpt, selOpt, masterOpt, queueOpt,
                       archiveOpt, stationOpt, objectOpt, sensorOpt});
    parser.process(app);

    if (!parser.isSet(serverOpt) || !parser.isSet(staOpt)) {
        std::fprintf(stderr, "tpslink: --server and --sta are required\n");
        return 2;
    }

    Options opt;
    opt.host = parser.value(serverOpt);
    if (int c = opt.host.indexOf(':'); c >= 0) {
        opt.port = opt.host.mid(c + 1).toUShort();
        opt.host = opt.host.left(c);
    }
    opt.net       = parser.value(netOpt);
    opt.sta       = parser.value(staOpt);
    opt.select    = parser.value(selOpt);
    opt.hasSelect = parser.isSet(selOpt);
    opt.station   = parser.value(stationOpt).toUInt();
    opt.object    = parser.value(objectOpt).toUInt();
    opt.sensor    = parser.value(sensorOpt).toUInt();
    opt.archiveDir = parser.value(archiveOpt);
    opt.archive    = parser.isSet(archiveOpt);

    tp::client::ApplicationSettings settings;
    settings.moduleName = "tpslink";
    settings.masterHost = parser.value(masterOpt);
    settings.queue      = parser.value(queueOpt);
    settings.sohIntervalSeconds = 2;

    SlinkApplication slink(std::move(settings), std::move(opt));
    return slink.exec();
}
