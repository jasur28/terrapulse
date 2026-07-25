#pragma once

#include "terrapulse/core/record.h"
#include "terrapulse/core/timewindow.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace tp::processing {

class StreamBuffer {
public:
    void setCapacity(std::size_t value) { m_capacity = value; trim(); }
    std::size_t capacity() const { return m_capacity; }
    std::size_t size() const { return m_records.size(); }

    void push(std::shared_ptr<tp::core::Record> record) {
        if (!record) {
            return;
        }
        m_records.push_back(std::move(record));
        trim();
    }

    std::vector<std::shared_ptr<tp::core::Record>> records(const std::string& streamID = {}) const {
        std::vector<std::shared_ptr<tp::core::Record>> out;
        for (const auto& record : m_records) {
            if (!record) {
                continue;
            }
            if (streamID.empty() || record->streamID() == streamID) {
                out.push_back(record);
            }
        }
        return out;
    }

    tp::core::TimeWindow timeWindow() const {
        if (m_records.empty() || !m_records.front() || !m_records.back()) {
            return {};
        }
        return {m_records.front()->startTime(), m_records.back()->endTime()};
    }

    void clear() { m_records.clear(); }

private:
    void trim() {
        while (m_records.size() > m_capacity) {
            m_records.pop_front();
        }
    }

    std::deque<std::shared_ptr<tp::core::Record>> m_records;
    std::size_t m_capacity = 4096;
};

} // namespace tp::processing
