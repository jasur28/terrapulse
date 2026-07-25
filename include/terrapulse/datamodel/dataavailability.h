#pragma once

#include "terrapulse/core/datetime.h"
#include "terrapulse/datamodel/publicobject.h"
#include "terrapulse/datamodel/waveformstreamid.h"

#include <memory>
#include <vector>

namespace tp::datamodel {

struct DataSegment {
    tp::core::Time start;
    tp::core::Time end;
    int sampleCount = 0;
};

class DataExtent : public PublicObject {
public:
    explicit DataExtent(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::DataExtent"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<DataExtent>(*this); }

    WaveformStreamID waveformID;
    tp::core::Time start;
    tp::core::Time end;
    double availability = 0.0;
    double latencySeconds = 0.0;
    std::vector<DataSegment> segments;
};

class DataAvailability : public PublicObject {
public:
    explicit DataAvailability(QString publicID = "TerraPulse/DataAvailability") : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::DataAvailability"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<DataAvailability>(*this); }

    bool add(std::shared_ptr<DataExtent> extent);
    const std::vector<std::shared_ptr<DataExtent>>& extents() const { return m_extents; }

private:
    std::vector<std::shared_ptr<DataExtent>> m_extents;
};

} // namespace tp::datamodel
