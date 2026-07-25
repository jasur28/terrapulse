#pragma once

#include "terrapulse/datamodel/comment.h"
#include "terrapulse/datamodel/publicobject.h"
#include "terrapulse/datamodel/waveformstreamid.h"

#include <QString>

#include <memory>
#include <vector>

namespace tp::datamodel {

class Channel : public PublicObject {
public:
    explicit Channel(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::Channel"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<Channel>(*this); }

    WaveformStreamID waveformID;
    double sampleRate = 0.0;
    QString unit = "m/s2";
    QString orientation;
};

class Sensor : public PublicObject {
public:
    explicit Sensor(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::Sensor"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<Sensor>(*this); }

    QString code;
    QString model;
    QString serialNumber;
    double latitude = 0.0;
    double longitude = 0.0;
    double elevation = 0.0;
    std::vector<std::shared_ptr<Channel>> channels;
};

class Structure : public PublicObject {
public:
    explicit Structure(QString publicID = {}) : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::Structure"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<Structure>(*this); }

    QString code;
    QString name;
    QString type;
    double latitude = 0.0;
    double longitude = 0.0;
    QString description;
    std::vector<Comment> comments;
    std::vector<std::shared_ptr<Sensor>> sensors;
};

class Inventory : public PublicObject {
public:
    explicit Inventory(QString publicID = "TerraPulse/Inventory") : PublicObject(std::move(publicID)) {}

    const char* className() const override { return "DataModel::Inventory"; }
    std::unique_ptr<Object> cloneObject() const override { return std::make_unique<Inventory>(*this); }

    bool addStructure(std::shared_ptr<Structure> structure);
    bool addSensor(const QString& structureID, std::shared_ptr<Sensor> sensor);
    Structure* structure(const QString& publicID) const;

    const std::vector<std::shared_ptr<Structure>>& structures() const { return m_structures; }

private:
    std::vector<std::shared_ptr<Structure>> m_structures;
};

} // namespace tp::datamodel
