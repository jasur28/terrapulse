#pragma once

#include "terrapulse/core/datetime.h"
#include "terrapulse/datamodel/publicobject.h"
#include "terrapulse/datamodel/waveformstreamid.h"

#include <QString>

namespace tp::datamodel {

class WaveformQuality : public PublicObject {
public:
    explicit WaveformQuality(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::WaveformQuality"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<WaveformQuality>(*this); }

    WaveformStreamID waveformID;
    QString type;
    QString parameter;
    double value = 0.0;
    double lowerUncertainty = 0.0;
    double upperUncertainty = 0.0;
    tp::core::Time start;
    tp::core::Time end;
};

} // namespace tp::datamodel
