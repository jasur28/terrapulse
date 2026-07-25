#pragma once

#include "terrapulse/core/timewindow.h"
#include "terrapulse/processing/waveformprocessor.h"

namespace tp::processing {

class TimeWindowProcessor : public WaveformProcessor {
public:
    void setTimeWindow(tp::core::TimeWindow window) { m_window = window; }
    const tp::core::TimeWindow& timeWindow() const { return m_window; }

protected:
    bool accepts(const tp::core::Record& record) const {
        return m_window.length().microseconds() <= 0
            || (!(record.endTime() < m_window.startTime()) && record.startTime() <= m_window.endTime());
    }

private:
    tp::core::TimeWindow m_window;
};

} // namespace tp::processing
