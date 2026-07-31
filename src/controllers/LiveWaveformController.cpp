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

    // Accumulate the sample into the pending batch (keep the whole shape). flush()
    // ships the batch and clears it; here we only append.
    if (s.px.empty()) s.batchT0 = tMs;   // timestamp of the first sample of this batch
    s.px.push_back(x); s.py.push_back(y); s.pz.push_back(z);

    s.x = x; s.y = y; s.z = z; s.t = tMs;   // latest (tiles / last-value consumers)
    if (rate > 0) s.rate = rate;
    s.lastAmp = std::max({std::fabs(x), std::fabs(y), std::fabs(z)});
    ++s.records;
    ++m_records;
    s.fresh = true;
    m_sinceLast.restart();

    // Bound memory if the UI ever stalls between flushes: keep at most kMaxBatch
    // pending, dropping the oldest and advancing t0 so the batch stays contiguous.
    // flush() runs every 80 ms, so this cap is only a safety net.
    constexpr std::size_t kMaxBatch = 8000;   // ~40 s @ 200 Hz
    if (s.px.size() > kMaxBatch) {
        const std::size_t drop = s.px.size() - kMaxBatch;
        s.px.erase(s.px.begin(), s.px.begin() + drop);
        s.py.erase(s.py.begin(), s.py.begin() + drop);
        s.pz.erase(s.pz.begin(), s.pz.begin() + drop);
        if (s.rate > 0) s.batchT0 += static_cast<qint64>(drop * 1000.0 / s.rate);
    }
}

void LiveWaveformController::flush() {
    bool any = false;
    for (auto& kv : m_map) {
        Stream& s = kv.second;
        if (!s.fresh || s.px.empty()) continue;
        s.fresh = false;
        any = true;

        // The whole pending batch — the waveform shape (all samples since last flush).
        const int n = static_cast<int>(s.px.size());
        QVariantList xs, ys, zs;
        xs.reserve(n); ys.reserve(n); zs.reserve(n);
        for (int i = 0; i < n; ++i) { xs.append(s.px[i]); ys.append(s.py[i]); zs.append(s.pz[i]); }

        QVariantMap batch;
        batch["station"] = s.object;              // station defaults to object (numeric id)
        batch["object"]  = s.object;
        batch["sensor"]  = s.sensor;
        batch["rate"]    = s.rate > 0 ? s.rate : 200;
        batch["t0"]      = static_cast<qlonglong>(s.batchT0);
        batch["n"]       = n;
        batch["xs"]      = xs;
        batch["ys"]      = ys;
        batch["zs"]      = zs;
        emit batchReceived(batch);

        // Latest single sample too — monitoring tiles still consume this shape.
        QVariantMap sample;
        sample["station"]     = s.object;
        sample["object"]      = s.object;
        sample["sensor"]      = s.sensor;
        sample["timestampMs"] = static_cast<qlonglong>(s.t);
        sample["sampleRate"]  = s.rate > 0 ? s.rate : 200;
        sample["x"]           = s.x;
        sample["y"]           = s.y;
        sample["z"]           = s.z;
        emit sampleReceived(sample);

        s.px.clear(); s.py.clear(); s.pz.clear();
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
