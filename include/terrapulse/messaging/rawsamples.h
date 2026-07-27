#pragma once

#include <QVariantList>
#include <QVariantMap>

// One place that reads a raw. waveform message as individual samples, so every
// consumer (tpproc, tpqc, tpwfparam, tpslinkserver, the console) handles both
// message shapes identically:
//   * batched  — arrays xs/ys/zs + t (first-sample time) + sampleRate  (tpacq)
//   * scalar   — single x/y/z + t                                       (tpslink)
//
// Batching cuts the message rate ~N-fold on the bus (see docs/МАСШТАБИРОВАНИЕ §3a)
// while consumers stay unchanged in behaviour: they still see one sample at a time.
namespace tp::messaging {

// Call fn(x, y, z, tMs, index) for each sample in the message, oldest first.
// In a batch, per-sample time is t0 + index * (1000 / sampleRate).
template <class Fn>
inline void forEachSample(const QVariantMap& h, Fn&& fn) {
    const double  rate = h.value("sampleRate").toDouble() > 0 ? h.value("sampleRate").toDouble() : 200.0;
    const qint64  t0   = h.value("t").toLongLong();

    if (h.contains("xs")) {                       // batched form
        const QVariantList xs = h.value("xs").toList();
        const QVariantList ys = h.value("ys").toList();
        const QVariantList zs = h.value("zs").toList();
        const int n = xs.size();
        for (int i = 0; i < n; ++i) {
            const qint64 t = t0 + static_cast<qint64>(i * 1000.0 / rate);
            fn(xs[i].toDouble(),
               i < ys.size() ? ys[i].toDouble() : 0.0,
               i < zs.size() ? zs[i].toDouble() : 0.0,
               t, i);
        }
    } else {                                       // scalar form
        fn(h.value("x").toDouble(), h.value("y").toDouble(), h.value("z").toDouble(), t0, 0);
    }
}

// Number of samples carried by a raw message (1 for the scalar form).
inline int sampleCount(const QVariantMap& h) {
    return h.contains("xs") ? h.value("xs").toList().size() : 1;
}

} // namespace tp::messaging
