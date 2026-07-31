#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

// Derives QObject (no Q_OBJECT: it adds no signals/slots of its own) purely so it
// can moveToThread() — the GUI runs its instance on a worker thread so the blocking
// handshake and miniSEED decode never touch the UI event loop. Its QTcpSocket is a
// child, so it moves with the object. Headless consumers just use it single-threaded.
class WaveformClient : public QObject {
public:
    // station, object, sensor, x, y, z, first-sample time (ms), sample rate.
    using TripleFn = std::function<void(uint32_t, uint32_t, uint32_t,
                                        double, double, double, int64_t, double)>;

    // One re-interleaved triaxial sample. Batches of these are delivered per network
    // read so a threaded consumer marshals once per read, not once per sample.
    struct Triple {
        uint32_t station, object, sensor;
        double   x, y, z;
        int64_t  t;
        double   rate;
    };
    using BatchFn = std::function<void(const std::vector<Triple>&)>;

    WaveformClient(QString host, quint16 port, QObject* parent = nullptr);

    // Per-sample sink (headless consumers). Called for every triple at the end of a
    // read. onBatch takes precedence when set (fewer cross-thread hops for the GUI).
    void onTriple(TripleFn fn) { m_onTriple = std::move(fn); }
    void onBatch(BatchFn fn)   { m_onBatch = std::move(fn); }
    // Map "<network>_<station>" -> objectId so FDSN-named streams resolve to a
    // numeric object (sensor is the numeric location). Legacy numeric ids need
    // no map. See tp::loadStationMap.
    void setStationMap(std::unordered_map<std::string, uint32_t> m) { m_stationMap = std::move(m); }
    // Subscribe to a subset of stations (multi-station SeedLink), so several
    // consumers can split the network between them. Each spec is "STA" (network
    // defaults to TP) or "NET.STA" / "NET_STA". Empty = all channels (uni-station).
    void setStations(const QStringList& stations) { m_stations = stations; }
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

    QString     m_host;
    quint16     m_port;
    QTcpSocket* m_sock = nullptr;   // child of this (moves with moveToThread)
    QByteArray  m_buf;
    TripleFn    m_onTriple;
    BatchFn     m_onBatch;
    std::vector<Triple> m_pending;   // triples of the current read, delivered at end
    std::unordered_map<std::string, uint32_t> m_stationMap;
    QStringList m_stations;                 // multi-station subset (empty = all)
    double     m_lastRate = 200.0;
    quint32    m_lastSeq = 0;
    bool       m_haveSeq = false;
    bool       m_wired = false;
    quint64    m_records = 0;
    quint64    m_unresolved = 0;
};

} // namespace tp::slink
