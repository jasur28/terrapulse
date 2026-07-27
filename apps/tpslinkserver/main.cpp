// tpslinkserver — SeedLink server (Stage 5). A separate process (АРХИТЕКТУРА
// §16): it subscribes to raw samples on the bus, packs them into miniSEED
// records, keeps them in a retention ring, and serves them over TCP to any
// SeedLink client (our tpslink, or ObsPy/SeisComp).
//
// This is slice 2: the TCP server + the minimal handshake + real-time
// streaming from a per-client cursor. Resume (DATA <seq>) and multi-station
// selectors are slice 3. Protocol: docs/SEEDLINK-PROTOCOL.md.
//
//   tpslinkserver [--master host] [--port 18000] [--record-samples n] [--ring n]

#include "slink/SeedLinkRing.h"
#include "mseed/Mseed.h"
#include "terrapulse/client/application.h"
#include "terrapulse/messaging/rawsamples.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace {

using tp::slink::SeedLinkRing;

class SlinkServerApp : public tp::client::Application {
public:
    SlinkServerApp(tp::client::ApplicationSettings settings,
                   quint16 port, int recordSamples, std::size_t ringPackets, quint16 feedPort)
        : Application(std::move(settings)), m_ring(ringPackets),
          m_port(port), m_recordSamples(recordSamples), m_feedPort(feedPort) {}

    bool init() override {
        if (!Application::init()) return false;

        QObject::connect(&m_server, &QTcpServer::newConnection, [this] { accept(); });
        if (!m_server.listen(QHostAddress::Any, m_port)) {
            std::fprintf(stderr, "[tpslinkserver] cannot listen on port %u: %s\n",
                         m_port, m_server.errorString().toUtf8().constData());
            return false;
        }
        // Push freshly buffered records to connected clients a few times a second.
        QObject::connect(&m_pump, &QTimer::timeout, [this] { pumpClients(); });
        m_pump.start(100);

        // Acquisition feed (Level 2, АРХИТЕКТУРА §16): acquisition pushes ready
        // 512-byte miniSEED records straight into the ring, broker-independent —
        // the waveform backbone is fed BEFORE the message bus, not after it.
        if (m_feedPort) {
            QObject::connect(&m_feedServer, &QTcpServer::newConnection, [this] { acceptFeed(); });
            if (!m_feedServer.listen(QHostAddress::Any, m_feedPort)) {
                std::fprintf(stderr, "[tpslinkserver] cannot listen on feed port %u: %s\n",
                             m_feedPort, m_feedServer.errorString().toUtf8().constData());
                return false;
            }
            std::printf("[tpslinkserver] acquisition feed on :%u (records -> ring, no broker)\n",
                        m_feedPort);
        }

        std::printf("[tpslinkserver] SeedLink on :%u   raw <- %s   rec=%d\n",
                    m_port, messagingUrl().toUtf8().constData(), m_recordSamples);
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["clients"]  = int(m_clients.size());
        c["packets"]  = static_cast<qulonglong>(m_ring.produced());
        c["channels"] = int(m_chans.size());
        return c;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (!topic.startsWith("raw.")) return;
        const quint32 obj = h.value("object").toUInt();
        const quint32 sen = h.value("sensor").toUInt();
        const double  rate= h.value("sampleRate").toDouble() > 0 ? h.value("sampleRate").toDouble() : 200.0;
        tp::messaging::forEachSample(h, [&](double x, double y, double z, qint64 t, int) {
            const double xyz[3] = { x, y, z };
            for (int comp = 0; comp < 3; ++comp)
                feed(obj, sen, comp, static_cast<int32_t>(std::lround(xyz[comp] * 10000.0)), t, rate);
        });
    }

private:
    struct Chan {
        std::string          sid;
        double               rate = 0.0;
        int64_t              startMs = 0;
        std::vector<int32_t> buf;
    };
    // One STATION subscription within a connection (multi-station): a station to
    // accept, plus the channel selectors that apply to it.
    struct StationSub { QByteArray net, sta; QList<QByteArray> selectors; };

    struct Client {
        QTcpSocket*        sock = nullptr;
        bool               streaming = false;
        uint32_t           cursor = 0;         // next sequence to send
        QByteArray         in;                 // handshake line buffer
        QList<StationSub>  stations;           // multi-station subscriptions
        QList<QByteArray>  uniSelectors;       // selectors when no STATION was given
        int                current = -1;       // station being configured (index)
    };

    // Trim SEED space-padding and compare a requested code with a packet field.
    static bool codeEq(const QByteArray& want, const char* field, int n) {
        QByteArray got(field, n);
        return want == got.trimmed();
    }

    // Does this client accept the packet? Handles uni-station (global selectors)
    // and multi-station (per-station selectors), matching net/sta/loc/chan at the
    // fixed miniSEED v2 offsets inside the SL packet.
    bool accept(const Client* cl, const char* pkt) const {
        const char* rec = pkt + tp::slink::kHeaderSize;   // miniSEED fixed header
        const char* sta = rec + 8;                        // station (5)
        const char* net = rec + 18;                       // network (2)
        const char* lc  = rec + 13;                       // location+channel (5)
        if (cl->stations.isEmpty()) return selected(lc, cl->uniSelectors);
        for (const StationSub& s : cl->stations)
            if (codeEq(s.net, net, 2) && codeEq(s.sta, sta, 5) && selected(lc, s.selectors))
                return true;
        return false;
    }

    // Match a packet's location+channel (5 chars) against a SeedLink selector.
    // Selector is 3 chars (channel) or 5 chars (location+channel); '?' is a
    // wildcard. A leading '!' negates. Empty selector list means accept all.
    static bool selected(const char* locChan5, const QList<QByteArray>& sels) {
        if (sels.isEmpty()) return true;
        bool anyPositive = false, matchedPositive = false;
        for (const QByteArray& s0 : sels) {
            bool neg = s0.startsWith('!');
            QByteArray s = neg ? s0.mid(1) : s0;
            const char* field = (s.size() == 5) ? locChan5 : locChan5 + 2;  // 5=>loc+chan, else chan
            const int   n     = (s.size() == 5) ? 5 : 3;
            if (s.size() != 5 && s.size() != 3) continue;
            bool m = true;
            for (int i = 0; i < n; ++i)
                if (s[i] != '?' && s[i] != field[i]) { m = false; break; }
            if (neg) { if (m) return false; }        // negative match excludes
            else     { anyPositive = true; matchedPositive |= m; }
        }
        return anyPositive ? matchedPositive : true; // only negatives => accept rest
    }

    static uint64_t key(quint32 o, quint32 s, int c) {
        return (uint64_t(o) << 20) | (uint64_t(s) << 4) | uint64_t(c & 0xF);
    }

    void feed(quint32 obj, quint32 sen, int comp, int32_t v, int64_t t, double rate) {
        Chan& c = m_chans[key(obj, sen, comp)];
        if (c.buf.empty()) {
            c.sid = tp::mseed::sourceId(obj, sen, comp);   // legacy id for slice 2
            c.rate = rate;
            c.startMs = t;
        }
        c.buf.push_back(v);
        if (int(c.buf.size()) >= m_recordSamples) flushChan(c);
    }

    void flushChan(Chan& c) {
        if (c.buf.empty()) return;
        tp::mseed::encode(c.sid, c.rate, c.startMs, c.buf,
                          [this](const char* rec, int len) {
                              if (len == tp::slink::kRecordSize) m_ring.push(rec);
                          });
        c.buf.clear();
    }

    void accept() {
        while (QTcpSocket* sock = m_server.nextPendingConnection()) {
            auto* cl = new Client{sock, false, 0, {}};
            m_clients.push_back(cl);
            QObject::connect(sock, &QTcpSocket::readyRead, [this, cl] { onRead(cl); });
            QObject::connect(sock, &QTcpSocket::disconnected, [this, cl] { drop(cl); });
            std::printf("[tpslinkserver] client connected (%d total)\n", int(m_clients.size()));
            std::fflush(stdout);
        }
    }

    // Acquisition feed: a producer streams back-to-back 512-byte miniSEED records;
    // each complete record goes straight into the ring. No handshake — this is the
    // trusted local acquisition path, not a client.
    void acceptFeed() {
        while (QTcpSocket* sock = m_feedServer.nextPendingConnection()) {
            auto* buf = new QByteArray();
            QObject::connect(sock, &QTcpSocket::readyRead, [this, sock, buf] {
                buf->append(sock->readAll());
                while (buf->size() >= tp::slink::kRecordSize) {
                    m_ring.push(buf->constData());
                    buf->remove(0, tp::slink::kRecordSize);
                }
            });
            QObject::connect(sock, &QTcpSocket::disconnected, [sock, buf] {
                sock->deleteLater(); delete buf;
            });
            std::printf("[tpslinkserver] acquisition feed connected\n");
            std::fflush(stdout);
        }
    }

    void drop(Client* cl) {
        m_clients.removeOne(cl);
        cl->sock->deleteLater();
        delete cl;
    }

    void onRead(Client* cl) {
        if (cl->streaming) { cl->sock->readAll(); return; }   // client stays silent while streaming
        cl->in += cl->sock->readAll();
        int nl;
        while ((nl = cl->in.indexOf('\n')) >= 0) {
            QByteArray line = cl->in.left(nl);
            cl->in.remove(0, nl + 1);
            if (line.endsWith('\r')) line.chop(1);
            handleCommand(cl, line.trimmed());
            if (cl->streaming) break;   // handshake done, rest is binary
        }
    }

    void handleCommand(Client* cl, const QByteArray& line) {
        if (line.isEmpty()) return;
        const int sp = line.indexOf(' ');
        const QByteArray cmd = (sp < 0 ? line : line.left(sp)).toUpper();
        std::printf("[tpslinkserver] <- '%s' (cmd '%s')\n",
                    line.constData(), cmd.constData());
        std::fflush(stdout);
        auto ok  = [&] { cl->sock->write("OK\r\n"); };
        if (cmd == "HELLO") {
            cl->sock->write("TerraPulse SeedLink v0.1\r\nTerraPulse\r\n");
        } else if (cmd == "SELECT") {
            const QByteArray arg = (sp < 0) ? QByteArray() : line.mid(sp + 1).trimmed().toUpper();
            auto& sels = (cl->current >= 0) ? cl->stations[cl->current].selectors : cl->uniSelectors;
            if (arg.isEmpty()) sels.clear();        // SELECT alone resets
            else               sels.append(arg);
            ok();
        } else if (cmd == "STATION") {
            // STATION <sta> <net> — begin a per-station subscription.
            const QList<QByteArray> parts = line.mid(sp + 1).trimmed().toUpper().split(' ');
            StationSub s;
            s.sta = parts.value(0);
            s.net = parts.value(1);
            cl->stations.append(s);
            cl->current = cl->stations.size() - 1;
            ok();
        } else if (cmd == "CAT") {
            ok();
        } else if (cmd == "DATA" || cmd == "FETCH" || cmd == "END") {
            ok();
            // "DATA <seq>" resumes from a hex sequence; bare "DATA" is real-time.
            const QByteArray arg = (sp < 0) ? QByteArray() : line.mid(sp + 1).trimmed();
            if (!arg.isEmpty()) {
                bool okHex = false;
                const uint32_t req = uint32_t(arg.toUInt(&okHex, 16) & tp::slink::kSeqMask);
                char tmp[tp::slink::kPacketSize];
                const auto r = m_ring.packet(req, tmp);
                const char* how;
                if (r == tp::slink::ReadResult::Ok) {
                    cl->cursor = req; how = "backfill";              // still in the ring
                } else if (req == m_ring.nextSeq()) {
                    cl->cursor = req; how = "caught up";             // wait for the next one
                } else if (r == tp::slink::ReadResult::TooOld) {
                    cl->cursor = m_ring.oldestSeq(); how = "too old, backfilled oldest";
                } else {
                    // Unknown seq (e.g. our in-memory ring reset on restart) —
                    // don't stall waiting for a number that will never come.
                    cl->cursor = m_ring.nextSeq(); how = "stale seq, real-time";
                }
                std::printf("[tpslinkserver] client -> resume from seq %u (asked %u, %s)\n",
                            cl->cursor, req, how);
            } else {
                cl->cursor = m_ring.nextSeq();      // real-time: start from now
                std::printf("[tpslinkserver] client -> streaming from seq %u\n", cl->cursor);
            }
            cl->streaming = true;
            std::fflush(stdout);
        } else if (cmd == "BYE") {
            cl->sock->disconnectFromHost();
        } else {
            cl->sock->write("ERROR\r\n");
        }
    }

    void pumpClients() {
        char pkt[tp::slink::kPacketSize];
        for (Client* cl : m_clients) {
            if (!cl->streaming) continue;
            for (;;) {
                const auto r = m_ring.packet(cl->cursor, pkt);
                if (r == tp::slink::ReadResult::Ok) {
                    // location(2)+channel(3) sit at fixed offsets in the miniSEED
                    // v2 fixed header, which starts 8 bytes into the SL packet.
                    if (accept(cl, pkt))
                        cl->sock->write(pkt, tp::slink::kPacketSize);
                    cl->cursor = (cl->cursor + 1) & tp::slink::kSeqMask;
                } else if (r == tp::slink::ReadResult::TooOld) {
                    cl->cursor = m_ring.nextSeq();   // fell behind: resync to newest
                    break;
                } else {                             // Future: nothing new yet
                    break;
                }
            }
        }
    }

    SeedLinkRing m_ring;
    QTcpServer   m_server;
    QTcpServer   m_feedServer;      // acquisition record feed (Level 2)
    QTimer       m_pump;
    QList<Client*> m_clients;
    std::unordered_map<uint64_t, Chan> m_chans;
    quint16 m_port;
    int     m_recordSamples;
    quint16 m_feedPort;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpslinkserver");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse SeedLink server");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    QCommandLineOption queueOpt ("queue", "Queue: production | playback", "name", "production");
    QCommandLineOption portOpt  ("port",  "TCP port to serve SeedLink on", "port", "18000");
    QCommandLineOption recOpt   ("record-samples", "miniSEED samples per record before publishing", "n", "500");
    QCommandLineOption ringOpt  ("ring",  "Retention: number of recent packets kept for resume", "n", "10000");
    QCommandLineOption feedOpt  ("feed-port", "Accept a direct acquisition record feed on this port "
                                 "(waveform backbone, no broker). 0 = disabled, use the raw. bus.", "port", "0");
    parser.addOptions({masterOpt, queueOpt, portOpt, recOpt, ringOpt, feedOpt});
    parser.process(app);

    const quint16 feedPort = quint16(parser.value(feedOpt).toUInt());

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpslinkserver";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = parser.value(queueOpt);
    // With a direct acquisition feed the backbone is broker-independent; without
    // it, fall back to consuming the raw. bus (Level 1 behaviour).
    settings.subscriptions = feedPort ? QStringList{} : QStringList{"raw."};
    settings.sohIntervalSeconds = 2;
    settings.pollIntervalMs = 10;   // raw rate: keep latency low

    SlinkServerApp server(std::move(settings),
                          quint16(parser.value(portOpt).toUInt()),
                          std::max(8, parser.value(recOpt).toInt()),
                          std::max<std::size_t>(16, parser.value(ringOpt).toULongLong()),
                          feedPort);
    return server.exec();
}
