#pragma once

#include "terrapulse/io/recordstream.h"

#include <deque>

namespace tp::io {

class MemoryRecordStream : public RecordStream {
public:
    bool setSource(const QString& source) override;
    void close() override;
    bool addStream(const QString& networkCode, const QString& stationCode,
                   const QString& locationCode, const QString& channelCode) override;
    bool addStream(const QString& networkCode, const QString& stationCode,
                   const QString& locationCode, const QString& channelCode,
                   std::optional<tp::core::Time> startTime,
                   std::optional<tp::core::Time> endTime) override;
    std::unique_ptr<tp::core::Record> next() override;

    void push(std::unique_ptr<tp::core::Record> record);
    int size() const { return static_cast<int>(m_records.size()); }

private:
    std::deque<std::unique_ptr<tp::core::Record>> m_records;
    bool m_closed = false;
};

} // namespace tp::io
