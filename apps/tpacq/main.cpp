// tpacq — TerraPulse acquisition daemon.
// Reads one accelerometer from a serial port and publishes each sample to the
// bus as a `raw.<station>.<object>.<sensor>` message (x/y/z carried in the CBOR
// header). No GUI. This is the standalone replacement for the in-app serial path.
//
// --sim: no hardware needed — generates a synthetic waveform (structural sine +
// noise) at --rate Hz. Handy for pipeline/broker tests and demos.
//
// The UART parsing lives untouched in SerialStreamReceiver (CRC-16/CCITT,
// 43-byte packets, DTR/RTS reset pulse); this file only wires it to the bus.

#include "serial/SerialStreamReceiver.h"
#include "mseed/TdsArchive.h"
#include "mseed/StreamId.h"
#include "mseed/Mseed.h"
#include "terrapulse/client/application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QStringList>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QVariantList>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Rec { qint64 t; double x, y, z; };

struct Config {
    quint32 station = 1, object = 1, sensor = 1;
    quint32 simRate = 200;
    bool    useSim = false, simEvents = false;
    bool    useReplay = false, historic = false;
    double  speed = 1.0;
    int     baud = 460800;
    QString port, replayFile, recordFile, archiveDir;
    bool    record = false, archive = false;
    // FDSN identity for this raw source. When stationCode is set, the archive is
    // named by the computed StreamId (band/instrument code from kind + rate);
    // empty keeps the legacy numeric id, so existing setups are unchanged.
    QString network = "TP", stationCode, kind = "accelerometer";
    double  cornerPeriod = 1e9;   // >=10 s => broadband band code (accelerometer)
    int     recordSamples = 1000; // miniSEED buffer before flush (fill vs latency)
    int     batch = 20;           // bus samples per message (1 = per-sample)
    QString slinkHost;            // --slink host:port: push miniSEED records to a
    quint16 slinkPort = 0;        // tpslinkserver feed (waveform backbone, no bus)
    bool    noBus = false;        // --no-bus: don't publish raw. to the broker at
                                  // all (pure backbone; only when no GUI needs it)
    int     feedBacklog = 20000;  // --feed-backlog: max records held during a feed
};                                // outage before the oldest are dropped (counted)

class AcqApplication : public tp::client::Application {
public:
    AcqApplication(tp::client::ApplicationSettings settings, Config cfg)
        : Application(std::move(settings)), m_cfg(std::move(cfg)),
          m_topic("raw." + std::to_string(m_cfg.station) + "." + std::to_string(m_cfg.object)
                        + "." + std::to_string(m_cfg.sensor)) {}

    bool init() override {
        if (!Application::init()) return false;    // publisher (+ store-forward) ready

        if (m_cfg.record) {
            m_recFile.setFileName(m_cfg.recordFile);
            if (m_recFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
                m_recOut.setDevice(&m_recFile);
            else
                std::printf("[tpacq] cannot open --record file '%s'\n",
                            m_cfg.recordFile.toUtf8().constData());
        }
        if (m_cfg.archive) {
            m_tds = std::make_unique<tp::mseed::TdsArchive>(
                m_cfg.archiveDir.toStdString(), m_cfg.recordSamples);
            if (!bindArchiveIdentity()) return false;   // refuse rather than archive nothing
        }
        if (m_cfg.slinkPort) {
            // Resolve the feed's channel identity the SAME way the archive does:
            // FDSN StreamId when a station code is configured, else legacy numeric.
            if (!m_cfg.stationCode.isEmpty()) {
                const auto kind = m_cfg.kind == "seismometer"
                                      ? tp::mseed::Instrument::Seismometer
                                      : tp::mseed::Instrument::Accelerometer;
                for (int comp = 0; comp < 3; ++comp) {
                    const auto r = tp::mseed::makeStreamId(
                        m_cfg.network.toStdString(), m_cfg.stationCode.toStdString(),
                        m_cfg.sensor, kind, m_cfg.simRate, m_cfg.cornerPeriod, comp);
                    if (!r.ok) {   // same hard rule as the archive: reject at input
                        std::fprintf(stderr, "[tpacq] FATAL: cannot build feed identity "
                                     "(comp %d): %s\n", comp, r.error.c_str());
                        return false;
                    }
                    m_feedSid[comp] = r.id.sourceId();
                }
            }
            connectFeed();
            QObject::connect(&m_feedSock, &QTcpSocket::connected,    [this]() { drainFeed(); });
            QObject::connect(&m_feedSock, &QTcpSocket::disconnected, [this]() {
                ++m_reconnects;
                QTimer::singleShot(2000, [this]() { connectFeed(); });   // reconnect
            });
            std::printf("[tpacq] waveform feed -> %s:%u (records, no bus, backlog no-loss)\n",
                        m_cfg.slinkHost.toUtf8().constData(), m_cfg.slinkPort);
        }

        m_serial.setBaudRate(m_cfg.baud);
        QObject::connect(&m_serial, &SerialStreamReceiver::sampleReceived,
                         [this](const QVariantMap& s) {
            publishSample(s.value("timestampMs").toLongLong(),
                          s.value("x").toDouble(), s.value("y").toDouble(), s.value("z").toDouble(),
                          s.value("sequence").toULongLong(), s.value("sampleRate").toUInt());
        });

        if (m_cfg.useSim)         startSim();
        else if (m_cfg.useReplay) startReplay();

        QObject::connect(&m_statsTimer, &QTimer::timeout, [this]() { printStats(); });
        m_statsTimer.start(2000);

        announce();
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["published"] = static_cast<qulonglong>(m_published);
        c["packets"]   = static_cast<qulonglong>(m_serial.packetCount());
        c["bad"]       = static_cast<qulonglong>(m_serial.badPacketCount());   // crcBad
        c["seqLost"]   = static_cast<qulonglong>(m_seqLost);
        c["src"]       = srcLabel();
        c["queue"]     = settings().queue;
        if (m_cfg.slinkPort) {
            c["feedBacklog"] = static_cast<qulonglong>(m_feedBacklog.size());
            c["feedDropped"] = static_cast<qulonglong>(m_feedDropped);
            c["reconnects"]  = static_cast<qulonglong>(m_reconnects);
        }
        return c;
    }

private:
    const char* srcLabel() const {
        return m_cfg.useReplay ? "replay" : m_cfg.useSim ? "sim" : "serial";
    }

    // Resolve and VALIDATE this raw source's archive identity at startup, and
    // bind the three axes. Returns false on an identity that miniSEED v2 cannot
    // represent — so the operator learns immediately, instead of the archive
    // silently staying empty for months (АРХИТЕКТУРА §14, "reject at input").
    bool bindArchiveIdentity() {
        if (!m_tds) return true;

        // Legacy numeric naming (no station code): station=object, location=
        // sensor. miniSEED v2 location is 2 chars, so sensor must be 0..99.
        if (m_cfg.stationCode.isEmpty()) {
            if (m_cfg.sensor > 99) {
                std::fprintf(stderr,
                    "[tpacq] FATAL: --archive with sensor id %u cannot be written "
                    "to miniSEED v2 (location max 99). Give an FDSN --station-code, "
                    "or use a sensor id <= 99.\n", m_cfg.sensor);
                return false;
            }
            return true;   // legacy id is valid
        }

        // FDSN naming: makeStreamId enforces every v2 field width.
        const auto kind = m_cfg.kind == "seismometer"
                              ? tp::mseed::Instrument::Seismometer
                              : tp::mseed::Instrument::Accelerometer;
        for (int comp = 0; comp < 3; ++comp) {
            const auto r = tp::mseed::makeStreamId(
                m_cfg.network.toStdString(), m_cfg.stationCode.toStdString(),
                m_cfg.sensor, kind, m_cfg.simRate, m_cfg.cornerPeriod, comp);
            if (!r.ok) {
                std::fprintf(stderr, "[tpacq] FATAL: cannot build archive identity "
                             "(comp %d): %s\n", comp, r.error.c_str());
                return false;
            }
            m_tds->setSourceId(m_cfg.object, m_cfg.sensor, comp, r.id.sourceId());
            std::printf("[tpacq] channel %d -> %s\n", comp, r.id.sourceId().c_str());
        }
        std::fflush(stdout);
        return true;
    }

    // Single publish path shared by every source.
    void publishSample(qint64 t, double x, double y, double z, quint64 seq, quint32 rate) {
        // Sequence-gap counter at the acquisition edge: a jump in the device's
        // own sample number means the link (not the analysis) lost samples.
        if (m_haveDevSeq && seq > m_lastDevSeq + 1) m_seqLost += seq - m_lastDevSeq - 1;
        m_lastDevSeq = seq; m_haveDevSeq = true;

        if (m_recOut.device())
            m_recOut << t << ',' << x << ',' << y << ',' << z << '\n';
        if (m_tds) {                       // gal -> integer counts (x10000), 3 axes
            m_tds->addSample(m_cfg.object, m_cfg.sensor, 0, static_cast<int32_t>(std::lround(x * 10000.0)), t, rate);
            m_tds->addSample(m_cfg.object, m_cfg.sensor, 1, static_cast<int32_t>(std::lround(y * 10000.0)), t, rate);
            m_tds->addSample(m_cfg.object, m_cfg.sensor, 2, static_cast<int32_t>(std::lround(z * 10000.0)), t, rate);
        }
        if (m_cfg.slinkPort) {             // waveform backbone: pack records, push to feed
            feedRecord(0, static_cast<int32_t>(std::lround(x * 10000.0)), t, rate);
            feedRecord(1, static_cast<int32_t>(std::lround(y * 10000.0)), t, rate);
            feedRecord(2, static_cast<int32_t>(std::lround(z * 10000.0)), t, rate);
        }
        // Batch the BUS publish: accumulate N samples into one raw message to cut
        // the message rate ~N-fold (docs/МАСШТАБИРОВАНИЕ §3a). The archive above
        // stays per-sample; consumers read either shape via forEachSample.
        if (m_batch.x.isEmpty()) { m_batch.t0 = t; m_batch.seq0 = seq; m_batch.rate = rate; }
        m_batch.x.append(x);
        m_batch.y.append(y);
        m_batch.z.append(z);
        if (m_batch.x.size() >= m_cfg.batch) flushBatch();
    }

    // Publish the accumulated samples as one batched raw message.
    void flushBatch() {
        if (m_batch.x.isEmpty()) return;
        if (m_cfg.noBus) {                 // pure backbone: nothing on the broker
            m_published += m_batch.x.size();
            m_batch.x.clear(); m_batch.y.clear(); m_batch.z.clear();
            return;
        }
        QVariantMap h;
        h["v"]          = 1;
        h["type"]       = "raw";
        h["station"]    = m_cfg.station;
        h["object"]     = m_cfg.object;
        h["sensor"]     = m_cfg.sensor;
        h["t"]          = static_cast<qlonglong>(m_batch.t0);   // time of first sample
        h["xs"]         = m_batch.x;
        h["ys"]         = m_batch.y;
        h["zs"]         = m_batch.z;
        h["seq"]        = static_cast<qulonglong>(m_batch.seq0);
        h["sampleRate"] = m_batch.rate;
        publish(m_topic, h);
        m_published += m_batch.x.size();
        m_batch.x.clear(); m_batch.y.clear(); m_batch.z.clear();
    }

    // Waveform backbone feed (--slink): per-component record accumulation + socket.
    struct FeedChan { std::string sid; double rate = 0.0; int64_t startMs = 0; std::vector<int32_t> buf; };
    std::unordered_map<int, FeedChan> m_feed;
    std::string m_feedSid[3];               // resolved FDSN id per component (empty = legacy)
    QTcpSocket  m_feedSock;
    std::deque<QByteArray> m_feedBacklog;   // records waiting to go out (no-loss on reconnect)
    quint64 m_feedDropped = 0, m_reconnects = 0, m_seqLost = 0;
    quint64 m_lastDevSeq = 0; bool m_haveDevSeq = false;

    void connectFeed() {
        m_feedSock.abort();
        m_feedSock.connectToHost(m_cfg.slinkHost, m_cfg.slinkPort);
    }

    // ── Waveform backbone feed: pack samples into miniSEED records and push them
    // straight to the tpslinkserver feed, bypassing the message bus (Level 2). ──
    void feedRecord(int comp, int32_t v, int64_t t, double rate) {
        FeedChan& c = m_feed[comp];
        if (c.buf.empty()) {
            c.sid = m_feedSid[comp].empty()
                        ? tp::mseed::sourceId(m_cfg.object, m_cfg.sensor, comp)
                        : m_feedSid[comp];               // FDSN id when resolved (see init)
            c.rate = rate; c.startMs = t;
        }
        c.buf.push_back(v);
        if (int(c.buf.size()) >= m_cfg.recordSamples) flushFeedChan(c);
    }

    // Encode a full window into records and QUEUE them in the backlog. Records
    // are never dropped on a disconnect — they wait in a bounded backlog and go
    // out on reconnect (no-loss within the backlog window).
    void flushFeedChan(FeedChan& c) {
        if (c.buf.empty()) return;
        tp::mseed::encode(c.sid, c.rate, c.startMs, c.buf,
                          [this](const char* rec, int len) {
                              if (len != 512) return;
                              m_feedBacklog.emplace_back(rec, 512);
                              while (m_feedBacklog.size() > std::size_t(m_cfg.feedBacklog)) {
                                  m_feedBacklog.pop_front();   // outage longer than the backlog
                                  ++m_feedDropped;
                              }
                          });
        c.buf.clear();
        drainFeed();
    }

    // Send as much of the backlog as the socket will take, oldest first.
    // QTcpSocket::write() copies the whole buffer into its own write queue and
    // returns the full length (or -1 on error) — it never does a partial write —
    // so a record is either fully queued (pop it) or the link errored (stop and
    // wait for the reconnect; the record stays intact at the front of backlog).
    void drainFeed() {
        if (m_feedSock.state() != QAbstractSocket::ConnectedState) return;
        while (!m_feedBacklog.empty()) {
            const QByteArray& rec = m_feedBacklog.front();
            if (m_feedSock.write(rec) != rec.size()) break;   // error: keep record, retry later
            m_feedBacklog.pop_front();
        }
    }

    // ── Synthetic source (--sim) ─────────────────────────────────────────────
    void startSim() {
        const int tickMs  = 10;
        const int perTick = std::max(1, int(m_cfg.simRate) * tickMs / 1000);
        const double f    = 3.1;                                    // structural tone (Hz)
        const double dph  = 2.0 * M_PI * f / double(m_cfg.simRate);
        QObject::connect(&m_simTimer, &QTimer::timeout, [this, perTick, dph]() {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            // --sim-events: every 20 s a ~3 s burst at ~10x amplitude trips an anomaly.
            const double amp = (m_cfg.simEvents && (now / 1000) % 20 < 3) ? 10.0 : 1.0;
            // Emit every sample that is DUE, tracked by a time cursor rather than a
            // fixed count per tick: the timer jitters, so a fixed count would leave
            // holes in the stream (QC saw them as gaps and availability < 100%).
            const double dtMs = 1000.0 / double(m_cfg.simRate);
            if (m_nextTs <= 0.0) m_nextTs = double(now);
            if (double(now) - m_nextTs > 2000.0) m_nextTs = double(now);   // resync after a stall
            int emitted = 0;
            const int maxPerTick = perTick * 20;                           // bound the catch-up
            while (m_nextTs <= double(now) && emitted < maxPerTick) {
                const qint64 ts = qint64(std::llround(m_nextTs));
                const double n = double(std::rand() % 1000 - 500) / 1000.0; // +/-0.5 noise
                const double x = amp * (5.0 * std::sin(m_ph)       + 0.8 * n);
                const double y = amp * (4.0 * std::sin(m_ph + 1.7) + 0.8 * n);
                const double z = 1000.0 + amp * (2.0 * std::sin(m_ph) + 0.5 * n); // ~1g bias on Z
                publishSample(ts, x, y, z, ++m_seq, m_cfg.simRate);
                m_ph += dph;
                if (m_ph > 2.0 * M_PI) m_ph -= 2.0 * M_PI;
                m_nextTs += dtMs;
                ++emitted;
            }
        });
        m_simTimer.start(tickMs);
    }

    // ── Replay source (--replay file) ────────────────────────────────────────
    void startReplay() {
        QFile f(m_cfg.replayFile);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                const QStringList p = in.readLine().split(',');
                if (p.size() >= 4)
                    m_recs.push_back({ p[0].toLongLong(), p[1].toDouble(), p[2].toDouble(), p[3].toDouble() });
            }
        }
        const quint32 rate    = std::max(1u, m_cfg.simRate);
        const int     tickMs  = 10;
        const int     perTick = std::max(1, int(std::llround(rate * m_cfg.speed * tickMs / 1000.0)));
        QObject::connect(&m_replayTimer, &QTimer::timeout, [this, perTick, rate]() {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            for (int i = 0; i < perTick && m_idx < m_recs.size(); ++i, ++m_idx) {
                const Rec& r = m_recs[m_idx];
                publishSample(m_cfg.historic ? r.t : now, r.x, r.y, r.z, ++m_seq, rate);
            }
            if (m_idx >= m_recs.size()) {
                std::printf("[tpacq] replay done (%zu samples)\n", m_recs.size());
                std::fflush(stdout);
                m_replayTimer.stop();
            }
        });
        // Start after a short delay so subscribers connect first (PUB/SUB slow-joiner);
        // a one-shot replay burst could otherwise finish before subscriptions propagate.
        QTimer::singleShot(1300, [this, tickMs]() { m_replayTimer.start(tickMs); });
    }

    void printStats() {
        flushBatch();                       // don't let a partial batch linger
        drainFeed();                        // and keep the feed backlog moving
        if (m_recOut.device()) m_recOut.flush();
        if (m_tds) m_tds->flushAll();
        if (m_cfg.slinkPort)
            std::printf("[tpacq] feed backlog=%zu dropped=%llu reconnects=%llu seqLost=%llu\n",
                        m_feedBacklog.size(),
                        static_cast<unsigned long long>(m_feedDropped),
                        static_cast<unsigned long long>(m_reconnects),
                        static_cast<unsigned long long>(m_seqLost));
        if (m_cfg.useSim || m_cfg.useReplay) {
            if (settings().storeForwardCap > 0 && publisher())
                std::printf("[tpacq] %s published=%llu  queue=%s  link=%s backlog=%zu\n",
                            srcLabel(), static_cast<unsigned long long>(m_published),
                            settings().queue.toUtf8().constData(),
                            publisher()->connected() ? "up" : "down", publisher()->backlog());
            else
                std::printf("[tpacq] %s published=%llu  queue=%s\n",
                            srcLabel(), static_cast<unsigned long long>(m_published),
                            settings().queue.toUtf8().constData());
        } else {
            std::printf("[tpacq] connected=%d bytes=%llu published=%llu packets=%llu bad=%llu\n",
                        m_serial.isConnected() ? 1 : 0,
                        static_cast<unsigned long long>(m_serial.bytesReceived()),
                        static_cast<unsigned long long>(m_published),
                        static_cast<unsigned long long>(m_serial.packetCount()),
                        static_cast<unsigned long long>(m_serial.badPacketCount()));
        }
        std::fflush(stdout);
    }

    void announce() {
        const QString url = messagingUrl();
        if (m_cfg.useReplay) {
            std::printf("[tpacq] REPLAY '%s' (%s)  ->  topic '%s'  via %s\n",
                        m_cfg.replayFile.toUtf8().constData(),
                        m_cfg.historic ? "historic" : "realtime",
                        m_topic.c_str(), url.toUtf8().constData());
        } else if (m_cfg.useSim) {
            std::printf("[tpacq] SIM %u Hz  ->  topic '%s'  via %s\n",
                        m_cfg.simRate, m_topic.c_str(), url.toUtf8().constData());
        } else if (m_cfg.port.isEmpty()) {
            std::printf("[tpacq] no --port given. Available ports: %s\n",
                        m_serial.availablePorts().join(", ").toUtf8().constData());
        } else if (m_serial.connectPort(m_cfg.port)) {
            std::printf("[tpacq] %s @ %d baud  ->  topic '%s'  via %s\n",
                        m_cfg.port.toUtf8().constData(), m_serial.baudRate(),
                        m_topic.c_str(), url.toUtf8().constData());
        } else {
            std::printf("[tpacq] failed to open %s: %s\n",
                        m_cfg.port.toUtf8().constData(),
                        m_serial.errorText().toUtf8().constData());
        }
        std::fflush(stdout);
    }

    Config m_cfg;
    std::string m_topic;

    SerialStreamReceiver m_serial;
    std::unique_ptr<tp::mseed::TdsArchive> m_tds;
    QFile m_recFile;
    QTextStream m_recOut;

    QTimer m_simTimer, m_replayTimer, m_statsTimer;
    std::vector<Rec> m_recs;
    std::size_t m_idx = 0;
    quint64 m_seq = 0, m_published = 0;

    // Pending bus batch (accumulated by publishSample, sent by flushBatch).
    struct Batch { QVariantList x, y, z; qint64 t0 = 0; quint64 seq0 = 0; quint32 rate = 200; };
    Batch m_batch;

    double  m_ph = 0.0, m_nextTs = 0.0;
};

} // namespace

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
    QCommandLineOption simEventsOpt("sim-events", "With --sim, inject a periodic anomaly burst (demo/testing)");
    QCommandLineOption rateOpt   ("rate", "Sample rate (Hz) for --sim / --replay", "hz", "200");
    QCommandLineOption replayOpt ("replay", "Replay a recorded CSV (t_ms,x,y,z) — usually to --queue playback", "file");
    QCommandLineOption historicOpt("historic", "Replay keeps original timestamps (default: retime to now)");
    QCommandLineOption speedOpt  ("speed", "Replay speed factor", "x", "1");
    QCommandLineOption recordOpt ("record", "Also append every published sample to a CSV (t_ms,x,y,z)", "file");
    QCommandLineOption archiveOpt("archive", "Also archive raw waveforms as miniSEED into this TDS directory", "dir");
    QCommandLineOption bufferOpt ("buffer",  "Store-and-forward: buffer this many seconds while the broker is down and resend on reconnect", "sec", "0");
    QCommandLineOption baudOpt   ("baud",    "Baud rate", "rate", "460800");
    QCommandLineOption stationOpt("station", "Station id", "id", "1");
    QCommandLineOption objectOpt ("object",  "Object id",  "id", "1");
    QCommandLineOption sensorOpt ("sensor",  "Sensor id",  "id", "1");
    QCommandLineOption netOpt    ("network",    "FDSN network code (<=2 chars)", "net", "TP");
    QCommandLineOption staCodeOpt("station-code","FDSN station code (<=5 chars); enables FDSN archive naming", "code");
    QCommandLineOption kindOpt   ("kind",       "Instrument: accelerometer | seismometer", "kind", "accelerometer");
    QCommandLineOption cornerOpt ("corner",     "Sensor corner period (s); >=10 => broadband band code", "sec", "1e9");
    QCommandLineOption recSampOpt("record-samples","miniSEED samples buffered before flush (fill vs latency)", "n", "1000");
    QCommandLineOption batchOpt  ("batch",       "Bus samples per message (cuts message rate; 1 = per-sample)", "n", "20");
    QCommandLineOption slinkOpt  ("slink",       "Push miniSEED records to a tpslinkserver feed host:port "
                                  "(waveform backbone, no broker)", "host:port", "");
    QCommandLineOption noBusOpt  ("no-bus",      "With --slink: do NOT publish raw. to the broker at all "
                                  "(pure backbone; use only when no GUI needs the bus)");
    QCommandLineOption backlogOpt("feed-backlog","Max miniSEED records held during a --slink outage "
                                  "before the oldest are dropped", "n", "20000");
    parser.addOptions({portOpt, masterOpt, queueOpt, simOpt, simEventsOpt, rateOpt, replayOpt, historicOpt,
                       speedOpt, recordOpt, archiveOpt, bufferOpt, baudOpt, stationOpt, objectOpt, sensorOpt,
                       netOpt, staCodeOpt, kindOpt, cornerOpt, recSampOpt, batchOpt, slinkOpt, noBusOpt, backlogOpt});
    parser.process(app);

    Config cfg;
    cfg.station    = parser.value(stationOpt).toUInt();
    cfg.object     = parser.value(objectOpt).toUInt();
    cfg.sensor     = parser.value(sensorOpt).toUInt();
    cfg.network     = parser.value(netOpt);
    cfg.stationCode = parser.value(staCodeOpt);
    cfg.kind        = parser.value(kindOpt);
    cfg.cornerPeriod= parser.value(cornerOpt).toDouble();
    cfg.recordSamples = std::max(8, parser.value(recSampOpt).toInt());
    cfg.batch         = std::max(1, parser.value(batchOpt).toInt());
    if (const QString sl = parser.value(slinkOpt); !sl.isEmpty()) {
        const int colon = sl.lastIndexOf(':');
        cfg.slinkHost = colon > 0 ? sl.left(colon) : sl;
        cfg.slinkPort = quint16(colon > 0 ? sl.mid(colon + 1).toUInt() : 18001);
    }
    cfg.noBus = parser.isSet(noBusOpt);
    cfg.feedBacklog = std::max(1, parser.value(backlogOpt).toInt());
    cfg.simRate    = std::max(1u, parser.value(rateOpt).toUInt());
    cfg.useSim     = parser.isSet(simOpt);
    cfg.simEvents  = parser.isSet(simEventsOpt);
    cfg.useReplay  = parser.isSet(replayOpt);
    cfg.historic   = parser.isSet(historicOpt);
    cfg.speed      = std::max(0.01, parser.value(speedOpt).toDouble());
    cfg.baud       = parser.value(baudOpt).toInt();
    cfg.port       = parser.value(portOpt);
    cfg.replayFile = parser.value(replayOpt);
    cfg.recordFile = parser.value(recordOpt);
    cfg.record     = parser.isSet(recordOpt);
    cfg.archiveDir = parser.value(archiveOpt);
    cfg.archive    = parser.isSet(archiveOpt);

    // Store-and-forward backlog cap: bufferSec seconds' worth (assume up to 200 Hz).
    const int bufferSec = parser.value(bufferOpt).toInt();
    const std::size_t sfCap =
        bufferSec > 0 ? std::size_t(bufferSec) * std::max(200u, cfg.simRate) : 0;

    tp::client::ApplicationSettings settings;
    settings.moduleName      = "tpacq";
    settings.masterHost      = parser.value(masterOpt);
    settings.queue           = parser.value(queueOpt);
    settings.sohIntervalSeconds = 2;
    settings.storeForwardCap = sfCap;      // acquisition is a pure publisher

    AcqApplication acq(std::move(settings), std::move(cfg));
    return acq.exec();
}
