#pragma once

#include "terrapulse/core/record.h"
#include "terrapulse/datamodel/waveformstreamid.h"

#include <QString>

#include <memory>
#include <vector>

namespace tp::gui {

class RecordViewItem {
public:
    tp::datamodel::WaveformStreamID streamID;
    QString label;
    bool enabled = true;
    std::vector<std::shared_ptr<tp::core::Record>> records;
};

} // namespace tp::gui
