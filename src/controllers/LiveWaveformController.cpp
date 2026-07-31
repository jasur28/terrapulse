#include "controllers/LiveWaveformController.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using Triple = tp::slink::WaveformClient::Triple;

LiveWaveformController::LiveWaveformController(const QString& host, quint16 port,
                                              std::unordered_map<std::string, uint32_t> stationMap,
                                              QStringList stations, QStringList selectors,
                                              QObject* parent)
    : QObject(parent)
{
    m_active = port != 0;
    if (!m_active) return;   // inactive: no endpoint, no client, no timers

    m_endpoint = host + ":" + QString::number(port);
    m_state    = "waiting";
    m_clock.start();

    m_client = std::make_unique<tp::slink::WaveformClient>(host, port);
    m_client->setStationMap(std::move(stationMap));
    // Optional subscription subset: pull only these stations / channels off the
    // backbone (SeedLink STATION + SELECT) instead of the whole network.
    if (!stations.isEmpty())  m_client->setStations(stations);
    if (!selectors.isEmpty()) m_client->setSelectors(selectors);
    // onBatch fires on the worker thread, once per network read. Everything heavy —
    // ring append AND decimation — happens there; only the finished envelope/streams/
    // latest are posted (queued) to this (GUI) thread. So m_map is worker-only and
    // needs no lock.
    m_client->onBatch([this](const std::vector<Triple>& batch) { ingestBatch(batch); });

    // Socket + handshake + decode + interleave + decimation all run on the worker.
    m_client->moveToThread(&m_thread);
    m_thread.start();

    // Only the "went quiet" check runs on the GUI thread.
    connect(&m_liveTimer, &QTimer::timeout, this, &LiveWaveformController::checkLiveness);
    m_liveTimer.start(1000);

    // Start the client inside its own thread, deferred so the window paints first;
    // the blocking connect+handshake then delays only the worker, never the UI.
    QTimer::singleShot(300, this, [this]() {
        tp::slink::WaveformClient* c = m_client.get();
        if (c) QMetaObject::invokeMethod(c, [c]() { c->start(); }, Qt::QueuedConnection);
    });
}

LiveWaveformController::~LiveWaveformController() {
    if (m_thread.isRunning()) {
        // Delete the client inside its own thread (correct affinity), then stop the
        // thread. BlockingQueued waits for the delete; the worker's loop is still
        // running, so there is no deadlock. After wait() the worker is gone, so m_map
        // has no concurrent access as the members destruct.
        if (m_client) {
            tp::slink::WaveformClient* c = m_client.release();
            QMetaObject::invokeMethod(c, [c]() { delete c; }, Qt::BlockingQueuedConnection);
        }
        m_thread.quit();
        m_thread.wait();
    }
}

// GUI thread (called from QML paint): the worker reads these next decimation.
void LiveWaveformController::setViewport(int cols, double windowSecs) {
    m_cols.store(std::clamp(cols, 64, 4000));
    m_windowMs.store(static_cast<qint64>(std::max(1.0, windowSecs) * 1000.0));
}

// Worker thread: append the batch to the per-stream rings, then (throttled) decimate
// and post the ready envelope + streams + latest samples to the GUI thread in one hop.
void LiveWaveformController::ingestBatch(const std::vector<Triple>& batch) {
    const qint64 windowMs = m_windowMs.load();
    for (const Triple& tr : batch) {
        Stream& s = m_map[(uint64_t(tr.object) << 32) | tr.sensor];
        s.object = tr.object; s.sensor = tr.sensor;

        // Cache the stream's real FDSN identity from the client (once it is known).
        if (m_client && (s.net.isEmpty() || s.chan[0].isEmpty()
                         || s.chan[1].isEmpty() || s.chan[2].isEmpty())) {
            tp::slink::WaveformClient::StreamIdentity id;
            if (m_client->identity(tr.object, tr.sensor, id)) {
                s.net = QString::fromStdString(id.net);
                s.sta = QString::fromStdString(id.sta);
                s.loc = QString::fromStdString(id.loc);
                for (int c = 0; c < 3; ++c)
                    if (!id.chan[c].empty()) s.chan[c] = QString::fromStdString(id.chan[c]);
            }
        }

        s.rt.push_back(tr.t);
        s.rx.push_back(static_cast<float>(tr.x));
        s.ry.push_back(static_cast<float>(tr.y));
        s.rz.push_back(static_cast<float>(tr.z));
        const qint64 cutoff = tr.t - windowMs - 2000;
        while (!s.rt.empty() && s.rt.front() < cutoff) {
            s.rt.pop_front(); s.rx.pop_front(); s.ry.pop_front(); s.rz.pop_front();
        }
        s.x = tr.x; s.y = tr.y; s.z = tr.z; s.t = tr.t;
        if (tr.rate > 0) s.rate = tr.rate;
        s.lastAmp = std::max({std::fabs(tr.x), std::fabs(tr.y), std::fabs(tr.z)});
        ++s.records;
    }
    m_records.fetch_add(batch.size());
    m_lastDataMs.store(m_clock.elapsed());

    // Throttle decimation to ~12/s regardless of how often reads arrive.
    const qint64 now = m_clock.elapsed();
    if (now - m_lastDecimMs < 80) return;
    m_lastDecimMs = now;

    QVariantList env     = buildEnvelopeList();
    QVariantList streams = buildStreamList();
    QVariantList latest  = buildLatestList();

    QMetaObject::invokeMethod(this, [this, env, streams, latest]() {
        m_envelopes = env;     emit envelopesChanged();
        m_streams   = streams; emit streamsChanged();
        for (const QVariant& v : latest) emit sampleReceived(v.toMap());
        if (!m_connected) { m_connected = true; m_state = "live"; emit stateChanged(); }
        emit statsChanged();
    }, Qt::QueuedConnection);
}

// Decimate each stream's rolling window to per-pixel columns the QML trace draws
// directly: per axis a min[]/max[] over `m_cols` columns plus mean/rms/peak for the
// auto-scale. Runs on the worker thread (off the UI), reading the worker-owned rings.
QVariantList LiveWaveformController::buildEnvelopeList() const {
    const int cols = std::max(1, m_cols.load());
    const qint64 windowMs = m_windowMs.load();
    QVariantList out;

    for (const auto& kv : m_map) {
        const Stream& s = kv.second;
        if (s.rt.empty()) continue;
        const qint64 tEnd = s.rt.back();
        const qint64 tStart = tEnd - windowMs;
        const double span = double(windowMs);

        const std::deque<float>* axes[3] = { &s.rx, &s.ry, &s.rz };
        double sum[3] = {0,0,0}, sumSq[3] = {0,0,0}, peak[3] = {1e-9,1e-9,1e-9};
        int used = 0;
        std::vector<float> mn[3], mx[3];
        for (int a = 0; a < 3; ++a) { mn[a].assign(cols, 1e30f); mx[a].assign(cols, -1e30f); }

        for (std::size_t i = 0; i < s.rt.size(); ++i) {
            if (s.rt[i] < tStart) continue;
            ++used;
            for (int a = 0; a < 3; ++a) sum[a] += (*axes[a])[i];
        }
        if (used == 0) continue;
        const double mean[3] = { sum[0]/used, sum[1]/used, sum[2]/used };

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
            cm["channel"] = s.chan[a].isEmpty() ? QString::fromLatin1(kChan[a]) : s.chan[a];
            cm["mean"] = mean[a];
            cm["rms"]  = std::sqrt(sumSq[a] / used);
            cm["peak"] = peak[a];
            cm["mn"]   = qmn;
            cm["mx"]   = qmx;
            chans.append(cm);
        }

        QVariantMap sm;
        sm["object"]      = s.object;
        sm["station"]     = s.object;                                    // numeric key (envIdx)
        sm["network"]     = s.net.isEmpty() ? QStringLiteral("TP") : s.net;
        sm["stationCode"] = s.sta.isEmpty() ? QString::number(s.object) : s.sta;
        sm["location"]    = s.loc;
        sm["sensor"]      = s.sensor;
        sm["rate"]        = s.rate;
        sm["cols"]        = cols;
        sm["chans"]       = chans;
        out.append(sm);
    }

    std::sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        if (ma["object"].toUInt() != mb["object"].toUInt())
            return ma["object"].toUInt() < mb["object"].toUInt();
        return ma["sensor"].toUInt() < mb["sensor"].toUInt();
    });
    return out;
}

// The set of streams arriving over the backbone — drives the trace rows.
QVariantList LiveWaveformController::buildStreamList() const {
    QVariantList list;
    for (const auto& kv : m_map) {
        const Stream& s = kv.second;
        QVariantMap m;
        m["object"]      = s.object;
        m["station"]     = s.object;                                    // numeric key (envIdx)
        m["network"]     = s.net.isEmpty() ? QStringLiteral("TP") : s.net;
        m["stationCode"] = s.sta.isEmpty() ? QString::number(s.object) : s.sta;
        m["location"]    = s.loc;
        m["sensor"]      = s.sensor;
        m["rate"]        = s.rate;
        QVariantList chs;
        for (int c = 0; c < 3; ++c) chs.append(s.chan[c]);
        m["channels"]    = chs;
        m["records"]     = static_cast<qulonglong>(s.records);
        m["lastAmp"]     = s.lastAmp;
        list.append(m);
    }
    std::sort(list.begin(), list.end(), [](const QVariant& a, const QVariant& b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        if (ma["object"].toUInt() != mb["object"].toUInt())
            return ma["object"].toUInt() < mb["object"].toUInt();
        return ma["sensor"].toUInt() < mb["sensor"].toUInt();
    });
    return list;
}

// Latest triaxial sample per stream — the monitoring tiles and the trace's time axis
// consume this shape (via sampleReceived), emitted on the GUI thread by ingestBatch.
QVariantList LiveWaveformController::buildLatestList() const {
    QVariantList list;
    for (const auto& kv : m_map) {
        const Stream& s = kv.second;
        if (s.rt.empty()) continue;
        QVariantMap sample;
        sample["station"]     = s.object;
        sample["object"]      = s.object;
        sample["sensor"]      = s.sensor;
        sample["timestampMs"] = static_cast<qlonglong>(s.t);
        sample["sampleRate"]  = s.rate > 0 ? s.rate : 200;
        sample["x"]           = s.x;
        sample["y"]           = s.y;
        sample["z"]           = s.z;
        list.append(sample);
    }
    return list;
}

void LiveWaveformController::checkLiveness() {
    if (m_connected && (m_clock.elapsed() - m_lastDataMs.load()) > 2500) {
        m_connected = false;
        m_state = "waiting";
        emit stateChanged();
    }
}
