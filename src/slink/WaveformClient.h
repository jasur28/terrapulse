#pragma once

#include <QByteArray>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

// WaveformClient — a live SeedLink client that a processing module uses instead
// of subscribing to raw. on the message bus. It connects to a tpslinkserver,
// receives 520-byte packets, decodes the miniSEED records, and re-interleaves
// the per-component records into triaxial samples per sensor, delivered through
// onTriple(). Reconnects and resumes with DATA <seq> on a dropped link.
//
// This is the Level-2 waveform backbone on the CONSUMER side (АРХИТЕКТУРА §16):
// waveforms travel SeedLink/records, the broker carries only computed results.
// Generalised from the single-sensor tpslink client to all sensors on one link
// (no STATION in the handshake = the server streams every channel).
namespace tp::slink {

class WaveformClient {
public:
    // station, object, sensor, x, y, z, first-sample time (ms), sample rate.
    using TripleFn = std::function<void(uint32_t, uint32_t, uint32_t,
                                        double, double, double, int64_t, double)>;

    WaveformClient(QString host, quint16 port);

    void onTriple(TripleFn fn) { m_onTriple = std::move(fn); }
    // Map "<network>_<station>" -> objectId so FDSN-named streams resolve to a
    // numeric object (sensor is the numeric location). Legacy numeric ids need
    // no map. See tp::loadStationMap.
    void setStationMap(std::unordered_map<std::string, uint32_t> m) { m_stationMap = std::move(m); }
    void start();                       // connect + handshake + auto-reconnect
    quint64 records()    const { return m_records; }
    quint64 unresolved() const { return m_unresolved; }   // records dropped: id not resolvable

private:
    void connectAndHandshake();
    void scheduleReconnect();
    void onReadyRead();
    void emitTriples(uint32_t object, uint32_t sensor);

    struct Axis { std::deque<std::pair<int64_t,int32_t>> q[3]; };  // per component
    std::unordered_map<uint64_t, Axis> m_axes;                    // key = obj<<32|sen

    QString    m_host;
    quint16    m_port;
    QTcpSocket m_sock;
    QByteArray m_buf;
    TripleFn   m_onTriple;
    std::unordered_map<std::string, uint32_t> m_stationMap;
    double     m_lastRate = 200.0;
    quint32    m_lastSeq = 0;
    bool       m_haveSeq = false;
    bool       m_wired = false;
    quint64    m_records = 0;
    quint64    m_unresolved = 0;
};

} // namespace tp::slink
