#include "proc/StreamMonitor.h"

#include <cmath>

namespace tp::proc {

StreamStatus StreamMonitor::feed(double value, int64_t tMs, double sampleRate) {
    ++m_samples;

    // Clipping is orthogonal to timing: a correctly-timed sample can still be
    // saturated. Count it either way; it becomes the status only if timing is Ok.
    const bool clip = m_clipLimit > 0.0 && std::fabs(value) >= m_clipLimit;
    if (clip) ++m_clipped;

    if (!m_have) {                       // first sample of this channel
        m_have = true;
        m_rate = sampleRate > 0.0 ? sampleRate : 0.0;
        m_lastT = tMs; m_lastValue = value;
        return clip ? StreamStatus::Clipped : StreamStatus::Ok;
    }

    // Sample-rate drift: reported rate departs from the established one by >1%.
    if (sampleRate > 0.0) {
        if (m_rate <= 0.0) m_rate = sampleRate;
        else if (std::fabs(sampleRate - m_rate) > 0.01 * m_rate) {
            ++m_rateChanges;
            m_rate = sampleRate;         // adopt the new rate going forward
            m_lastT = tMs; m_lastValue = value;
            return StreamStatus::RateChanged;
        }
    }

    const double period = m_rate > 0.0 ? 1000.0 / m_rate : 0.0;
    const int64_t dt = tMs - m_lastT;

    StreamStatus st = StreamStatus::Ok;
    if (dt < 0) {
        ++m_overlaps; st = StreamStatus::Overlap;
    } else if (dt == 0 && value == m_lastValue) {
        ++m_duplicates; st = StreamStatus::Duplicate;
    } else if (period > 0.0 && double(dt) > m_gapFactor * period) {
        ++m_gaps; st = StreamStatus::Gap;
    } else if (clip) {
        st = StreamStatus::Clipped;
    }

    m_lastT = tMs; m_lastValue = value;
    return st;
}

void StreamMonitor::reset() {
    m_rate = 0.0; m_lastT = 0; m_lastValue = 0.0; m_have = false;
    m_samples = m_gaps = m_overlaps = m_duplicates = m_clipped = m_rateChanges = 0;
}

} // namespace tp::proc
