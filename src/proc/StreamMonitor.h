#pragma once
#include <cstdint>

// StreamMonitor — the per-stream integrity layer of the processing framework
// (Поток 2). SeisComp's WaveformProcessor checks every record for gaps, overlaps,
// duplicates, sample-rate change and saturation before the applied processor
// runs; our modules analysed the raw stream trusting it was clean. This gives
// each channel one place to answer "is this sample trustworthy, and did the
// stream just break?" so a structural or strong-motion processor can reset its
// state on a gap and flag clipped/duplicated data.
//
// Sample-based (our pipeline delivers samples, not records): feed one sample of
// one channel; it returns a verdict and keeps running counters.
namespace tp::proc {

enum class StreamStatus {
    Ok,          // in sequence, in range
    Gap,         // time jumped forward beyond the tolerance
    Overlap,     // time went backwards
    Duplicate,   // same timestamp and same value as the previous sample
    Clipped,     // |value| reached the saturation limit
    RateChanged, // reported sample rate differs from the established one
};

class StreamMonitor {
public:
    // clipLimit: saturation magnitude in the sample's own units (0 = no clip
    // check). gapFactor: a forward jump beyond gapFactor sample periods is a gap.
    explicit StreamMonitor(double clipLimit = 0.0, double gapFactor = 1.5)
        : m_clipLimit(clipLimit), m_gapFactor(gapFactor) {}

    StreamStatus feed(double value, int64_t tMs, double sampleRate);

    uint64_t samples()     const { return m_samples; }
    uint64_t gaps()        const { return m_gaps; }
    uint64_t overlaps()    const { return m_overlaps; }
    uint64_t duplicates()  const { return m_duplicates; }
    uint64_t clipped()     const { return m_clipped; }
    uint64_t rateChanges() const { return m_rateChanges; }
    // Any integrity event since reset (gap/overlap/duplicate/clip/rate change).
    uint64_t faults() const { return m_gaps + m_overlaps + m_duplicates + m_clipped + m_rateChanges; }

    void reset();

private:
    double   m_clipLimit;
    double   m_gapFactor;
    double   m_rate = 0.0;        // established sample rate (0 = not yet)
    int64_t  m_lastT = 0;
    double   m_lastValue = 0.0;
    bool     m_have = false;

    uint64_t m_samples = 0, m_gaps = 0, m_overlaps = 0,
             m_duplicates = 0, m_clipped = 0, m_rateChanges = 0;
};

} // namespace tp::proc
