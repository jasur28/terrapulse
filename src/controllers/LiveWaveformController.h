#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace tp::slink { class WaveformClient; }

// LiveWaveformController — the GUI's live-waveform source, a RecordStream client
// rather than a message-bus subscriber. SeisComp's consoles read waveforms over
// RecordStream/SeedLink (separate from the messaging bus that carries results);
// TerraPulse follows the same split: analysis results reach the console over the
// broker (AppController), but the raw live traces come off the SeedLink backbone
// (tpslinkserver) through this controller. The old path — subscribing to `raw.`
// on the broker (BusClient) — was only ever a demo shortcut.
//
// It wraps tp::slink::WaveformClient (connect + handshake + decode + resume),
// decimates to one representative triaxial sample per stream per flush tick so
// the QML canvas stays light, and exposes what the trace view needs: the set of
// live `streams`, a running `records` count, and a `connectionState`. The UI does
// no processing — it only draws what arrives here.
class LiveWaveformController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString      endpoint        READ endpoint        CONSTANT)
    Q_PROPERTY(bool         connected       READ connected       NOTIFY stateChanged)
    Q_PROPERTY(QString      connectionState READ connectionState NOTIFY stateChanged)
    Q_PROPERTY(qulonglong   records         READ records         NOTIFY statsChanged)
    Q_PROPERTY(QVariantList streams         READ streams         NOTIFY streamsChanged)

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
    qulonglong   records()         const { return m_records; }
    QVariantList streams()         const { return m_streams; }

signals:
    // One decimated triaxial sample of one stream (same shape BusClient emitted, so
    // the trace view keys series by object/sensor unchanged): object, sensor,
    // station, timestampMs, sampleRate, x, y, z.
    void sampleReceived(const QVariantMap& sample);
    void stateChanged();
    void statsChanged();
    void streamsChanged();

private slots:
    void flush();         // emit each stream's newest sample, refresh the streams list
    void checkLiveness(); // flip connected/state when the feed goes quiet

private:
    struct Stream {
        quint32 object = 0, sensor = 0;
        double  x = 0, y = 0, z = 0;
        qint64  t = 0;
        double  rate = 0;
        double  lastAmp = 0;
        quint64 records = 0;
        bool    fresh = false;   // a new sample arrived since the last flush
    };

    void onTriple(uint32_t station, uint32_t object, uint32_t sensor,
                  double x, double y, double z, int64_t tMs, double rate);
    void rebuildStreams();

    std::unique_ptr<tp::slink::WaveformClient> m_client;
    std::unordered_map<uint64_t, Stream>       m_map;   // key = object<<32 | sensor

    QString       m_endpoint;
    QString       m_state = "off";
    QTimer        m_flushTimer;
    QTimer        m_liveTimer;
    QElapsedTimer m_sinceLast;

    bool         m_active    = false;
    bool         m_connected = false;
    qulonglong   m_records   = 0;
    QVariantList m_streams;
};
