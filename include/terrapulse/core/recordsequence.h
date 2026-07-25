#pragma once

#include "terrapulse/core/record.h"

#include <memory>
#include <vector>

namespace tp::core {

class RecordSequence {
public:
    void add(std::unique_ptr<Record> record) { m_records.push_back(std::move(record)); }
    int size() const { return static_cast<int>(m_records.size()); }
    const std::vector<std::unique_ptr<Record>>& records() const { return m_records; }

private:
    std::vector<std::unique_ptr<Record>> m_records;
};

} // namespace tp::core
