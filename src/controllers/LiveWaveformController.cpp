#include "controllers/LiveWaveformController.h"
#include "slink/WaveformClient.h"

#include <algorithm>
#include <cmath>
#include <vector>

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
    // The batch sink fires on the worker thread (below), once per network read,
    // where decode/interleave run. Copy the batch and marshal ONE queued event back
    // to THIS object's thread — not one per sample — so onTriple()/flush() touch the
    // rings from one thread only (no locks) and the GUI sees few cross-thread hops
    // even with many sensors.
    m_client->onBatch([this](const std::vector<tp::slink::WaveformClient::Triple>& batch) {
        auto copy = std::make_shared<std::vector<tp::slink::WaveformClient::Triple>>(batch);
        QMetaObject::invokeMethod(this, [this, copy]() {
            for (const auto& tr : *copy)
                onTriple(tr.station, tr.object, tr.sensor, tr.x, tr.y, tr.z, tr.t, tr.rate);
        }, Qt::QueuedConnection);
    });

    // Run the SeedLink socket, the (blocking) handshake and the miniSEED decode on a
    // worker thread — a RecordStreamThread analog — so none of it touches the UI
    // event loop. The client's QTcpSocket is its child, so it moves along.
    m_client->moveToThread(&m_thread);
    m_thread.start();

    // flush() and checkLiveness() run on THIS (GUI) thread, draining the rings the
    // worker fills. flush cadence unchanged.
    connect(&m_flushTimer, &QTimer::timeout, this, &LiveWaveformController::flush);
    m_flushTimer.start(80);

    connect(&m_liveTimer, &QTimer::timeout, this, &LiveWaveformController::checkLiveness);
    m_liveTimer.start(1000);
    m_sinceLast.start();

    // Start the client inside its own thread, deferred so the window paints first;
    // the blocking connect+handshake now delays only the worker, never the UI.
    QTimer::singleShot(300, this, [this]() {
        tp::slink::WaveformClient* c = m_client.get();
        if (c) QMetaObject::invokeMethod(c, [c]() { c->start(); }, Qt::QueuedConnection);
    });
}

LiveWaveformController::~LiveWaveformController() {
    if (m_thread.isRunning()) {
        // Delete the client inside its own thread (correct affinity), then stop the
        // thread. BlockingQueued waits for the delete; the worker's loop is still
        // running, so there is no deadlock.
        if (m_client) {
            tp::slink::WaveformClient* c = m_client.release();
            QMetaObject::invokeMethod(c, [c]() { delete c; }, Qt::BlockingQueuedConnection);
        }
        m_thread.quit();
        m_thread.wait();
    }
}

// Called from WaveformClient's reader (main thread): update per-stream state.
// No emit here — flush() batches UI notifications at a fixed cadence.
void LiveWaveformController::setViewport(int cols, double windowSecs) {
    const int c = std::clamp(cols, 64, 4000);
    const qint64 w = static_cast<qint64>(std::max(1.0, windowSecs) * 1000.0);
    if (c == m_cols && w == m_windowMs) return;
    m_cols = c; m_windowMs = w;
}

void LiveWaveformController::onTriple(uint32_t /*station*/, uint32_t object, uint32_t sensor,
                                      double x, double y, double z, int64_t tMs, double rate) {
    Stream& s = m_map[(uint64_t(object) << 32) | sensor];
    s.object = object; s.sensor = sensor;

    // Append to the rolling window (one time deque shared by the three axes) and
    // trim anything older than the window (plus a small margin). The envelope is
    // decimated from this ring in flush().
    s.rt.push_back(tMs);
    s.rx.push_back(static_cast<float>(x));
    s.ry.push_back(static_cast<float>(y));
    s.rz.push_back(static_cast<float>(z));
    const qint64 cutoff = tMs - m_windowMs - 2000;
    while (!s.rt.empty() && s.rt.front() < cutoff) {
        s.rt.pop_front(); s.rx.pop_front(); s.ry.pop_front(); s.rz.pop_front();
    }

    s.x = x; s.y = y; s.z = z; s.t = tMs;   // latest (tiles / last-value consumers)
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

        // Latest single sample — monitoring tiles consume this. The trace view reads
        // the decimated `envelopes` instead (built once below, not per sample).
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
    }
    if (!any) return;

    if (!m_connected) { m_connected = true; m_state = "live"; emit stateChanged(); }
    buildEnvelopes();
    rebuildStreams();
    emit statsChanged();
}

// Decimate each stream's rolling window to per-pixel columns the QML trace can draw
// directly: for each of the three axes, a min[] and max[] over `m_cols` columns plus
// the mean/rms/peak used to auto-scale. The O(N) decimation runs in C++ (not QML JS),
// though still on the GUI thread — moving WaveformClient's socket+decode to a worker
// thread (a RecordStreamThread analog) is the remaining step off the UI thread.
void LiveWaveformController::buildEnvelopes() {
    const int cols = std::max(1, m_cols);
    QVariantList out;

    for (auto& kv : m_map) {
        Stream& s = kv.second;
        if (s.rt.empty()) continue;
        const qint64 tEnd = s.rt.back();
        const qint64 tStart = tEnd - m_windowMs;
        const double span = double(m_windowMs);

        // Per-axis accumulators over the visible window.
        const std::deque<float>* axes[3] = { &s.rx, &s.ry, &s.rz };
        double sum[3] = {0,0,0}, sumSq[3] = {0,0,0}, peak[3] = {1e-9,1e-9,1e-9};
        int used = 0;
        std::vector<float> mn[3], mx[3];
        for (int a = 0; a < 3; ++a) { mn[a].assign(cols, 1e30f); mx[a].assign(cols, -1e30f); }

        // Pass 1: means (need the mean before the deviation stats).
        for (std::size_t i = 0; i < s.rt.size(); ++i) {
            if (s.rt[i] < tStart) continue;
            ++used;
            for (int a = 0; a < 3; ++a) sum[a] += (*axes[a])[i];
        }
        if (used == 0) continue;
        const double mean[3] = { sum[0]/used, sum[1]/used, sum[2]/used };

        // Pass 2: deviation stats + per-column min/max.
        for (std::size_t i = 0; i < s.rt.size(); ++i) {
            if (s.rt[i] < tStart) continue;
            int b = int(double(s.rt[i] - tStart) / span * cols);
            if (b < 0) b = 0; else if (b >= cols) b = cols - 1;
            for (int a = 0; a < 3; ++a) {
                const float v = (*axes[a])[i];
                const double d = v - mean[a];
                sumSq[a] += d * d;
                const double ad = d < 0 ? -d : d;
                if (ad > peak[a]) peak[a] = ad;
                if (v < mn[a][b]) mn[a][b] = v;
                if (v > mx[a][b]) mx[a][b] = v;
            }
        }

        static const char* kChan[3] = { "HNX", "HNY", "HNZ" };
        QVariantList chans;
        for (int a = 0; a < 3; ++a) {
            QVariantList qmn, qmx; qmn.reserve(cols); qmx.reserve(cols);
            for (int b = 0; b < cols; ++b) {
                if (mx[a][b] < mn[a][b]) { qmn.append(QVariant()); qmx.append(QVariant()); }
                else { qmn.append(mn[a][b]); qmx.append(mx[a][b]); }
            }
            QVariantMap cm;
            cm["channel"] = QString::fromLatin1(kChan[a]);
            cm["mean"] = mean[a];
            cm["rms"]  = std::sqrt(sumSq[a] / used);
            cm["peak"] = peak[a];
            cm["mn"]   = qmn;
            cm["mx"]   = qmx;
            chans.append(cm);
        }

        QVariantMap sm;
        sm["object"]  = s.object;
        sm["station"] = s.object;
        sm["network"] = QStringLiteral("TP");
        sm["sensor"]  = s.sensor;
        sm["rate"]    = s.rate;
        sm["cols"]    = cols;
        sm["chans"]   = chans;
        out.append(sm);
    }

    std::sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        if (ma["object"].toUInt() != mb["object"].toUInt())
            return ma["object"].toUInt() < mb["object"].toUInt();
        return ma["sensor"].toUInt() < mb["sensor"].toUInt();
    });
    m_envelopes = out;
    emit envelopesChanged();
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
