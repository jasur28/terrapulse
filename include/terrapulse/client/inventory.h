#pragma once

#include "core/Inventory.h"

#include <map>
#include <utility>
#include <vector>

namespace tp::client {

struct SensorLocation {
    double latitude = 0.0;
    double longitude = 0.0;
    double elevation = 0.0;
};

using Structure = tp::inv::Structure;
using Sensor = tp::inv::Sensor;
using Channel = tp::inv::Channel;

class Inventory {
public:
    static Inventory& instance();
    static void reset();

    void setStructures(std::vector<Structure> structures);
    void setSensors(std::vector<Sensor> sensors);
    void setChannels(std::vector<Channel> channels);

    const Structure* structure(quint32 objectId) const;
    const Sensor* sensor(quint32 objectId, quint32 sensorId) const;
    const Channel* channel(quint32 objectId, quint32 sensorId, int component) const;
    SensorLocation location(quint32 objectId) const;

    const std::vector<Structure>& structures() const { return m_structures; }
    const std::vector<Sensor>& sensors() const { return m_sensors; }
    const std::vector<Channel>& channels() const { return m_channels; }

private:
    std::vector<Structure> m_structures;
    std::vector<Sensor> m_sensors;
    std::vector<Channel> m_channels;
};

} // namespace tp::client
