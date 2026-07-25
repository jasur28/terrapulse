#pragma once

#include "terrapulse/core/record.h"
#include "terrapulse/processing/processor.h"

namespace tp::processing {

class WaveformProcessor : public Processor {
public:
    virtual bool feed(const tp::core::Record& record) = 0;
};

} // namespace tp::processing
