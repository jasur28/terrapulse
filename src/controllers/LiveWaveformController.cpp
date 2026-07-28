#include "controllers/LiveWaveformController.h"
#include "slink/WaveformClient.h"

#include <algorithm>
#include <cmath>

LiveWaveformController::LiveWaveformController(const QString& host, quint16 port,
                                              std::unordered_map<std::string, uint32_t> stationMap,
                                              QObject* parent)
    : QObject(parent)
{
    m_active = port != 0;
    if (!m_active) return;   // inactive: no endpoint, no client, no timers

    m_endpoint = host + ":" + QString::number(port);
    m_state    = "waiting";

    m_client = std::make_unique<tp::slink::WaveformClient>(host, port);
    m_client->setStationMap(std::move(stationMap));
    m_client->onTriple([this](uint32_t sta, uint32_t obj, uint32_t sen,
                              double x, double y, double z, int64_t t, double rate) {
        onTriple(sta, obj, sen, x, y, z, t, rate);
    });

    // Decimate for the canvas: emit each stream's newest sample ~12×/s. The trace
    // ring keeps far fewer points than full rate, so one point per stream per tick
    // is plenty and keeps the UI responsive under many streams.
    connect(&m_flushTimer, &QTimer::timeout, this, &LiveWaveformController::flush);
    m_flushTimer.start(80);

    connect(&m_liveTimer, &QTimer::timeout, this, &LiveWaveformController::checkLiveness);
    m_liveTimer.start(1000);
    m_sinceLast.start();

    // Defer the (briefly blocking) connect+handshake so the window paints first;
    // if the backbone is down this only delays the reader, not the whole UI.
    QTimer::singleShot(300, this, [this]() { if (m_client) m_client->start(); });
}

LiveWaveformController::~LiveWaveformController() = default;

// Called from WaveformClient's reader (main thread): update per-stream state.
// No emit here — flush() batches UI notifications at a fixed cadence.
void LiveWaveformController::onTriple(uint32_t /*station*/, uint32_t object, uint32_t sensor,
                                      double x, double y, double z, int64_t tMs, double rate) {
    Stream& s = m_map[(uint64_t(object) << 32) | sensor];
    s.object = object; s.sensor = sensor;
    s.x = x; s.y = y; s.z = z; s.t = tMs;
    if (rate > 0) s.rate = rate;
    s.lastAmp = std::max({std::fabs(x), std::fabs(y), std::fabs(z)});
    ++s.records;
    ++m_records;
    s.fresh = true;
    m_sinceLast.restart();
}

void LiveWaveformController::flush() {
    bool any = false;
    for (auto& kv : m_map) {
        Stream& s = kv.second;
        if (!s.fresh) continue;
        s.fresh = false;
        any = true;

        QVariantMap sample;
        sample["station"]     = s.object;   // station defaults to object (numeric id)
        sample["object"]      = s.object;
        sample["sensor"]      = s.sensor;
        sample["timestampMs"] = static_cast<qlonglong>(s.t);
        sample["sampleRate"]  = s.rate > 0 ? s.rate : 200;
        sample["x"]           = s.x;
        sample["y"]           = s.y;
        sample["z"]           = s.z;
        emit sampleReceived(sample);
    }
    if (!any) return;

    if (!m_connected) { m_connected = true; m_state = "live"; emit stateChanged(); }
    rebuildStreams();
    emit statsChanged();
}

// The set of streams actually arriving over the backbone — this is what drives the
// trace rows, so the view reflects the record stream rather than static inventory.
void LiveWaveformController::rebuildStreams() {
    QVariantList list;
    for (const auto& kv : m_map) {
        const Stream& s = kv.second;
        QVariantMap m;
        m["object"]  = s.object;
        m["station"] = s.object;              // numeric id; network/station strings are future work
        m["network"] = QStringLiteral("TP");
        m["sensor"]  = s.sensor;
        m["rate"]    = s.rate;
        m["records"] = static_cast<qulonglong>(s.records);
        m["lastAmp"] = s.lastAmp;
        list.append(m);
    }
    std::sort(list.begin(), list.end(), [](const QVariant& a, const QVariant& b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        if (ma["object"].toUInt() != mb["object"].toUInt())
            return ma["object"].toUInt() < mb["object"].toUInt();
        return ma["sensor"].toUInt() < mb["sensor"].toUInt();
    });
    m_streams = list;
    emit streamsChanged();
}

void LiveWaveformController::checkLiveness() {
    if (m_connected && m_sinceLast.elapsed() > 2500) {
        m_connected = false;
        m_state = "waiting";
        emit stateChanged();
    }
}
