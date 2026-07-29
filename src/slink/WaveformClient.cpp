#include "slink/WaveformClient.h"
#include "slink/SeedLinkRing.h"
#include "mseed/Mseed.h"

#include <algorithm>
#include <cstdio>

namespace tp::slink {

WaveformClient::WaveformClient(QString host, quint16 port)
    : m_host(std::move(host)), m_port(port) {}

void WaveformClient::start() { connectAndHandshake(); }

void WaveformClient::scheduleReconnect() {
    QTimer::singleShot(2000, [this]() { connectAndHandshake(); });
}

// Blocking handshake with signals suppressed, then hand over to the async reader.
// Resumes with DATA <seq+1> so a dropped link does not lose the outage gap.
void WaveformClient::connectAndHandshake() {
    m_buf.clear();
    m_sock.blockSignals(true);
    m_sock.abort();
    m_sock.connectToHost(m_host, m_port);
    if (!m_sock.waitForConnected(5000)) {
        std::fprintf(stderr, "[waveform] cannot connect to %s:%u\n",
                     m_host.toUtf8().constData(), m_port);
        m_sock.blockSignals(false); scheduleReconnect(); return;
    }

    auto cmd = [&](const QByteArray& c) {
        m_sock.write(c + "\r\n"); m_sock.flush();
        m_sock.waitForReadyRead(3000);
        return m_sock.readAll();
    };
    cmd("HELLO");
    // Station selection (multi-station SeedLink): register each requested station
    // before DATA so several consumers can split the network. No STATION at all =
    // the server streams every channel (uni-station), the default.
    for (const QString& spec : m_stations) {
        QString net = QStringLiteral("TP"), sta = spec.trimmed();
        int sep = sta.indexOf('_');
        if (sep < 0) sep = sta.indexOf('.');
        if (sep >= 0) { net = sta.left(sep); sta = sta.mid(sep + 1); }
        if (sta.isEmpty()) continue;
        cmd(("STATION " + sta + " " + net).toLatin1());
    }
    QByteArray dataCmd = "DATA";
    if (m_haveSeq) {
        const quint32 next = (m_lastSeq + 1) & 0xFFFFFF;
        dataCmd += " " + QByteArray(QString("%1").arg(next, 6, 16, QChar('0')).toUpper().toLatin1());
    }
    if (!cmd(dataCmd).startsWith("OK")) {
        std::fprintf(stderr, "[waveform] DATA rejected by %s:%u\n",
                     m_host.toUtf8().constData(), m_port);
        m_sock.blockSignals(false); scheduleReconnect(); return;
    }
    m_sock.blockSignals(false);

    if (!m_wired) {
        QObject::connect(&m_sock, &QTcpSocket::readyRead,    [this]() { onReadyRead(); });
        QObject::connect(&m_sock, &QTcpSocket::disconnected, [this]() { scheduleReconnect(); });
        m_wired = true;
    }
    QTimer::singleShot(0, [this]() { onReadyRead(); });   // drain anything buffered
}

void WaveformClient::onReadyRead() {
    m_buf += m_sock.readAll();
    while (m_buf.size() >= kPacketSize) {
        if (m_buf[0] != 'S' || m_buf[1] != 'L') { m_buf.remove(0, 1); continue; }
        bool okHex = false;
        const quint32 seq = m_buf.mid(2, 6).toUInt(&okHex, 16);
        if (okHex) { m_lastSeq = seq; m_haveSeq = true; }
        const QByteArray rec = m_buf.mid(kHeaderSize, kRecordSize);
        m_buf.remove(0, kPacketSize);

        for (auto& d : tp::mseed::decode(rec.constData(), rec.size())) {
            ++m_records;
            if (d.sampleRate > 0) m_lastRate = d.sampleRate;
            uint32_t obj = 0, sen = 0; int comp = -1;
            if (!tp::mseed::resolveSourceId(d.sid, m_stationMap, obj, sen, comp) || comp < 0) {
                if (++m_unresolved <= 3)     // don't spam: warn on the first few only
                    std::fprintf(stderr, "[waveform] cannot resolve source id '%s' "
                                 "(FDSN station not in inventory map?) — record dropped\n",
                                 d.sid.c_str());
                continue;
            }
            auto& ax = m_axes[(uint64_t(obj) << 32) | sen];
            for (std::size_t i = 0; i < d.samples.size(); ++i)
                ax.q[comp].emplace_back(d.startTimeMs + int64_t(i * 1000.0 / d.sampleRate),
                                        d.samples[i]);
            emitTriples(obj, sen);
        }
    }
}

// Re-interleave the three per-component queues of one sensor into triaxial
// samples by matching timestamps (same tolerance approach as tpslink).
void WaveformClient::emitTriples(uint32_t object, uint32_t sensor) {
    auto& ax = m_axes[(uint64_t(object) << 32) | sensor];
    auto& X = ax.q[0]; auto& Y = ax.q[1]; auto& Z = ax.q[2];
    while (!X.empty() && !Y.empty() && !Z.empty()) {
        const int64_t tx = X.front().first, ty = Y.front().first, tz = Z.front().first;
        const int64_t tmin = std::min({tx, ty, tz}), tmax = std::max({tx, ty, tz});
        const double tol = m_lastRate > 0 ? 1000.0 / m_lastRate / 2 : 5.0;
        if (double(tmax - tmin) > tol) {                 // resync: drop the laggards
            if (tx < tmax - tol) X.pop_front();
            if (ty < tmax - tol) Y.pop_front();
            if (tz < tmax - tol) Z.pop_front();
            continue;
        }
        if (m_onTriple)
            m_onTriple(object, object, sensor,          // station defaults to object
                       X.front().second / 10000.0,
                       Y.front().second / 10000.0,
                       Z.front().second / 10000.0,
                       tx, m_lastRate);
        X.pop_front(); Y.pop_front(); Z.pop_front();
    }
}

} // namespace tp::slink
