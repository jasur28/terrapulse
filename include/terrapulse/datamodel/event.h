#pragma once

#include "bus/BusMessage.h"
#include "core/Types.h"
#include "terrapulse/datamodel/comment.h"
#include "terrapulse/datamodel/publicobject.h"
#include "terrapulse/datamodel/waveformstreamid.h"

#include <QString>

#include <memory>
#include <vector>

namespace tp::datamodel {

using EventMessage = tp::BusMessage;
using WarningLevel = tp::WarningLevel;
using SeverityLevel = tp::SeverityLevel;

class Event : public PublicObject {
public:
    explicit Event(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::Event"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<Event>(*this); }

    QString structureID;
    tp::AnomalyType type = tp::AnomalyType::None;
    tp::SeverityLevel severity = tp::SeverityLevel::Low;
    tp::AnomalyStatus status = tp::AnomalyStatus::Monitoring;
    tp::core::Time time = tp::core::Time::now();
    QString description;
    std::vector<WaveformStreamID> contributingStreams;
    std::vector<Comment> comments;
};

class EventParameters : public PublicObject {
public:
    explicit EventParameters(QString publicID = "TerraPulse/EventParameters") : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::EventParameters"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<EventParameters>(*this); }

    bool add(std::shared_ptr<Event> event);
    const std::vector<std::shared_ptr<Event>>& events() const { return m_events; }

private:
    std::vector<std::shared_ptr<Event>> m_events;
};

} // namespace tp::datamodel
