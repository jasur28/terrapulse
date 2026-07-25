#pragma once

#include "terrapulse/datamodel/publicobject.h"
#include "terrapulse/datamodel/waveformquality.h"

#include <memory>
#include <vector>

namespace tp::datamodel {

class QualityControl : public PublicObject {
public:
    explicit QualityControl(QString publicID = "TerraPulse/QualityControl") : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::QualityControl"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<QualityControl>(*this); }

    bool add(std::shared_ptr<WaveformQuality> quality);
    const std::vector<std::shared_ptr<WaveformQuality>>& waveformQualities() const { return m_waveformQualities; }

private:
    std::vector<std::shared_ptr<WaveformQuality>> m_waveformQualities;
};

} // namespace tp::datamodel
