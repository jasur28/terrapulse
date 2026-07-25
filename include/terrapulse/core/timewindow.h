#pragma once

#include "terrapulse/core/datetime.h"

namespace tp::core {

class TimeWindow {
public:
    TimeWindow() = default;
    TimeWindow(Time start, Time end) : m_start(start), m_end(end) {}
    TimeWindow(Time start, TimeSpan length) : m_start(start), m_end(start + length) {}

    Time startTime() const { return m_start; }
    Time endTime() const { return m_end; }
    TimeSpan length() const { return m_end - m_start; }

    bool contains(const Time& time) const { return !(time < m_start) && time <= m_end; }
    bool overlaps(const TimeWindow& other) const {
        return contains(other.m_start) || contains(other.m_end) || other.contains(m_start);
    }

private:
    Time m_start;
    Time m_end;
};

} // namespace tp::core
