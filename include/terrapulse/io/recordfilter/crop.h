#pragma once

#include "terrapulse/core/timewindow.h"
#include "terrapulse/io/recordfilter.h"

namespace tp::io {

class CropFilter : public RecordFilter {
public:
    explicit CropFilter(tp::core::TimeWindow window) : m_window(window) {}

    std::unique_ptr<tp::core::Record> feed(std::unique_ptr<tp::core::Record> record) override;
    std::unique_ptr<tp::core::Record> flush() override { return {}; }
    void reset() override {}
    std::unique_ptr<RecordFilter> clone() const override { return std::make_unique<CropFilter>(m_window); }

private:
    tp::core::TimeWindow m_window;
};

} // namespace tp::io
