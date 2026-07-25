#pragma once

#include "terrapulse/core/datetime.h"
#include "terrapulse/datamodel/waveformstreamid.h"
#include "terrapulse/datamodel/strongmotion/types.h"

namespace tp::datamodel::strongmotion {

struct PeakMotion {
    tp::datamodel::WaveformStreamID waveformID;
    Component component = Component::Z;
    tp::core::Time time = tp::core::Time::now();
    double pga = 0.0;
    double pgv = 0.0;
    double pgd = 0.0;
    double rms = 0.0;
};

} // namespace tp::datamodel::strongmotion
