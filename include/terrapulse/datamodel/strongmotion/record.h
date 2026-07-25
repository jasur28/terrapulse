#pragma once

#include "terrapulse/datamodel/publicobject.h"
#include "terrapulse/datamodel/strongmotion/filterparameter.h"
#include "terrapulse/datamodel/strongmotion/peakmotion.h"

#include <QString>

#include <vector>

namespace tp::datamodel::strongmotion {

class Record : public tp::datamodel::PublicObject {
public:
    explicit Record(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::StrongMotion::Record"; }
    std::unique_ptr<tp::datamodel::Object> cloneObject() const override { return std::make_unique<Record>(*this); }

    QString eventID;
    tp::datamodel::WaveformStreamID waveformID;
    tp::core::Time start;
    tp::core::Time end;
    double sampleRate = 0.0;
    int sampleCount = 0;
    QString fileURI;
    std::vector<FilterParameter> filters;
    std::vector<PeakMotion> peaks;
};

} // namespace tp::datamodel::strongmotion
