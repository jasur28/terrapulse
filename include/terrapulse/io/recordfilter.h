#pragma once

#include "terrapulse/core/record.h"

#include <memory>
#include <vector>

namespace tp::io {

class RecordFilter {
public:
    virtual ~RecordFilter() = default;
    virtual std::unique_ptr<tp::core::Record> feed(std::unique_ptr<tp::core::Record> record) = 0;
    virtual std::unique_ptr<tp::core::Record> flush() = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<RecordFilter> clone() const = 0;
};

class PassThroughRecordFilter : public RecordFilter {
public:
    std::unique_ptr<tp::core::Record> feed(std::unique_ptr<tp::core::Record> record) override { return record; }
    std::unique_ptr<tp::core::Record> flush() override { return {}; }
    void reset() override {}
    std::unique_ptr<RecordFilter> clone() const override { return std::make_unique<PassThroughRecordFilter>(); }
};

class RecordFilterChain : public RecordFilter {
public:
    void add(std::unique_ptr<RecordFilter> filter);
    std::unique_ptr<tp::core::Record> feed(std::unique_ptr<tp::core::Record> record) override;
    std::unique_ptr<tp::core::Record> flush() override;
    void reset() override;
    std::unique_ptr<RecordFilter> clone() const override;

private:
    std::vector<std::unique_ptr<RecordFilter>> m_filters;
};

} // namespace tp::io
