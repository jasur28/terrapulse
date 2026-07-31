#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

#include "slink/WaveformClient.h"   // Triple type (batch sink) + the client

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// LiveWaveformController — the GUI's live-waveform source, a RecordStream client
// rather than a message-bus subscriber. SeisComp's consoles read waveforms over
// RecordStream/SeedLink (separate from the messaging bus that carries results);
// TerraPulse follows the same split: analysis results reach the console over the
// broker (AppController), but the raw live traces come off the SeedLink backbone
// (tpslinkserver) through this controller. The old path — subscribing to `raw.`
// on the broker (BusClient) — was only ever a demo shortcut.
//
// It wraps tp::slink::WaveformClient, which runs on a worker thread (m_thread):
// the socket, the handshake, the miniSEED decode/interleave AND the trace
// decimation all happen there. Only a ready per-pixel envelope, the streams list
// and the latest samples are posted (queued) to the GUI thread. So the rings are
// touched from the worker thread alone and the GUI-visible members (m_envelopes,
// m_streams, m_state) from the GUI thread alone — no locks, shared scalars are
// atomic. The UI does no processing; it only draws the envelope that arrives here.
class LiveWaveformController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString      endpoint        READ endpoint        CONSTANT)
    Q_PROPERTY(bool         connected       READ connected       NOTIFY stateChanged)
    Q_PROPERTY(QString      connectionState READ connectionState NOTIFY stateChanged)
    Q_PROPERTY(qulonglong   records         READ records         NOTIFY statsChanged)
    Q_PROPERTY(QVariantList streams         READ streams         NOTIFY streamsChanged)
    // Per-stream decimated trace envelope, recomputed in C++ each flush so QML only
    // draws ready pixel columns (min/max/mean) — never iterates raw samples. See
    // envelopes() for the shape; setViewport() sets the pixel/second geometry.
    Q_PROPERTY(QVariantList envelopes       READ envelopes       NOTIFY envelopesChanged)

public:
    // host/port point at a tpslinkserver's SeedLink port (default 18000). port==0
    // leaves the controller inactive (endpoint empty, never connects) so main can
    // always construct it. stationMap resolves FDSN "<net>_<sta>" ids to numeric
    // objects (tp::loadStationMap); legacy numeric ids need no map.
    LiveWaveformController(const QString& host, quint16 port,
                           std::unordered_map<std::string, uint32_t> stationMap,
                           QObject* parent = nullptr);
    ~LiveWaveformController();

    QString      endpoint()        const { return m_endpoint; }
    bool         connected()       const { return m_connected; }
    QString      connectionState() const { return m_state; }
    qulonglong   records()         const { return m_records.load(); }
    QVariantList streams()         const { return m_streams; }
    QVariantList envelopes()       const { return m_envelopes; }

    // The trace view tells us its plot width (pixels) and time span (seconds); the
    // envelope is then decimated to exactly that many columns, so QML maps column i
    // straight to pixel i with no resampling. Called when the view geometry changes.
    Q_INVOKABLE void setViewport(int cols, double windowSecs);

signals:
    // One triaxial sample of one stream (the latest): object, sensor, station,
    // timestampMs, sampleRate, x, y, z. Consumed by the monitoring tiles / last-
    // value views. The trace view uses the decimated `envelopes` instead.
    void sampleReceived(const QVariantMap& sample);
    void envelopesChanged();
    void stateChanged();
    void statsChanged();
    void streamsChanged();

private slots:
    void checkLiveness(); // flip connected/state when the feed goes quiet (GUI thread)

private:
    struct Stream {
        quint32 object = 0, sensor = 0;
        double  x = 0, y = 0, z = 0;
        qint64  t = 0;
        double  rate = 0;
        double  lastAmp = 0;
        quint64 records = 0;
        // Rolling window of the last windowMs of samples (triaxial, one shared time
        // deque). Worker-thread only; the envelope is decimated from this.
        std::deque<qint64> rt;
        std::deque<float>  rx, ry, rz;
    };

    // --- worker thread (m_thread) ---
    void ingestBatch(const std::vector<tp::slink::WaveformClient::Triple>& batch);
    QVariantList buildEnvelopeList() const;  // decimate rings to per-pixel min/max/mean
    QVariantList buildStreamList()   const;  // the live streams for the trace rows
    QVariantList buildLatestList()   const;  // latest triaxial sample per stream (tiles)

    QThread                                    m_thread;   // runs socket+decode+decimation
    std::unique_ptr<tp::slink::WaveformClient> m_client;
    std::unordered_map<uint64_t, Stream>       m_map;   // key = object<<32 | sensor (worker)
    qint64        m_lastDecimMs = 0;                    // worker: throttle decimation

    QString       m_endpoint;
    QString       m_state = "off";     // GUI thread
    QTimer        m_liveTimer;         // GUI thread
    QElapsedTimer m_clock;             // monotonic, never reset (read from both threads)

    bool                  m_active    = false;
    bool                  m_connected = false;   // GUI thread
    std::atomic<qint64>   m_lastDataMs{0};       // worker writes, GUI reads (liveness)
    std::atomic<quint64>  m_records{0};
    QVariantList          m_streams;             // GUI thread
    QVariantList          m_envelopes;           // GUI thread

    // Trace viewport (set by QML on the GUI thread, read by the worker decimation).
    std::atomic<int>      m_cols{1000};          // pixel columns to decimate to
    std::atomic<qint64>   m_windowMs{60000};     // visible time span (ms)
};
