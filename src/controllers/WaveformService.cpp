#include "controllers/WaveformService.h"
#include "mseed/RecordStream.h"
#include "mseed/Mseed.h"

#include <QVariantList>
#include <algorithm>
#include <cmath>

namespace {

// Slice a trace to [startMs, endMs] and min/max-decimate to <= maxPoints so peaks
// survive. Appends physical-unit doubles (counts/10000 -> gal) to `out`.
void decimate(const tp::mseed::Trace& t, qlonglong startMs, qlonglong endMs,
              int maxPoints, QVariantList& out, double& mn, double& mx) {
    if (t.samples.empty() || t.sampleRate <= 0) return;
    const double dtMs = 1000.0 / t.sampleRate;
    long i0 = long(std::floor((startMs - t.startTimeMs) / dtMs));
    long i1 = long(std::ceil((endMs   - t.startTimeMs) / dtMs));
    i0 = std::max<long>(0, i0);
    i1 = std::min<long>(long(t.samples.size()), i1);
    if (i1 <= i0) return;

    const long span = i1 - i0;
    const int  buckets = int(std::min<long>(maxPoints, span));
    for (int b = 0; b < buckets; ++b) {
        const long a = i0 + long(std::llround(double(b)     * span / buckets));
        const long z = i0 + long(std::llround(double(b + 1) * span / buckets));
        double lo = 1e300, hi = -1e300;
        for (long k = a; k < z && k < i1; ++k) {
            const double v = t.samples[k] / 10000.0;
            lo = std::min(lo, v); hi = std::max(hi, v);
        }
        if (lo > hi) continue;
        // Emit the bucket extreme farthest from zero (preserves the visible peak).
        const double v = std::abs(hi) >= std::abs(lo) ? hi : lo;
        out.append(v);
        mn = std::min(mn, lo); mx = std::max(mx, hi);
    }
}

} // namespace

WaveformService::WaveformService(QString tdsRoot, QObject* parent)
    : QObject(parent), m_root(std::move(tdsRoot)) {}

QVariantMap WaveformService::load(int object, int sensor,
                                  qlonglong startMs, qlonglong endMs, int maxPoints) const {
    QVariantMap r;
    r["ok"] = false;
    r["object"] = object; r["sensor"] = sensor;
    r["startMs"] = startMs; r["endMs"] = endMs;
    if (m_root.isEmpty() || endMs <= startMs) return r;

    const std::string root = m_root.toStdString();
    double rate = 0.0;
    const char* keys[3] = { "x", "y", "z" };
    double gmin = 1e300, gmax = -1e300;
    int total = 0;

    for (int comp = 0; comp < 3; ++comp) {
        tp::mseed::Trace t = tp::mseed::readTdsChannel(root, tp::mseed::sourceId(object, sensor, comp));
        if (t.sampleRate > 0) rate = t.sampleRate;
        QVariantList pts;
        double mn = 1e300, mx = -1e300;
        decimate(t, startMs, endMs, maxPoints, pts, mn, mx);
        r[keys[comp]] = pts;
        r[QString(keys[comp]) + "Min"] = (mn <= mx) ? mn : 0.0;
        r[QString(keys[comp]) + "Max"] = (mn <= mx) ? mx : 0.0;
        total += pts.size();
        if (mn <= mx) { gmin = std::min(gmin, mn); gmax = std::max(gmax, mx); }
    }

    r["rate"] = rate;
    r["n"] = total;
    r["min"] = (gmin <= gmax) ? gmin : 0.0;
    r["max"] = (gmin <= gmax) ? gmax : 0.0;
    r["ok"] = total > 0;
    return r;
}
