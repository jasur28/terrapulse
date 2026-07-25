#pragma once

#include "terrapulse/datamodel/publicobject.h"
#include "terrapulse/datamodel/strongmotion/record.h"

#include <memory>
#include <vector>

namespace tp::datamodel::strongmotion {

class StrongMotionParameters : public tp::datamodel::PublicObject {
public:
    explicit StrongMotionParameters(QString publicID = "TerraPulse/StrongMotionParameters")
        : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::StrongMotion::StrongMotionParameters"; }
    std::unique_ptr<tp::datamodel::Object> cloneObject() const override {
        return std::make_unique<StrongMotionParameters>(*this);
    }

    QString eventID;
    QString structureID;
    bool add(std::shared_ptr<Record> record);
    const std::vector<std::shared_ptr<Record>>& records() const { return m_records; }

private:
    std::vector<std::shared_ptr<Record>> m_records;
};

} // namespace tp::datamodel::strongmotion
