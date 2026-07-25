#include "terrapulse/core/archive.h"
#include "terrapulse/core/baseobject.h"
#include "terrapulse/core/io.h"
#include "terrapulse/core/metaobject.h"
#include "terrapulse/core/record.h"

#include <QFile>
#include <QJsonValue>

#include <algorithm>

namespace tp::core {

MetaObject::MetaObject(std::string className, const MetaObject* base)
    : m_className(std::move(className)), m_base(base) {}

void MetaObject::addProperty(MetaProperty property) {
    m_properties.push_back(std::move(property));
}

const MetaProperty* MetaObject::property(int index) const {
    if (index < 0 || index >= propertyCount()) return nullptr;
    return &m_properties[static_cast<std::size_t>(index)];
}

const MetaProperty* MetaObject::property(const std::string& name) const {
    auto it = std::find_if(m_properties.begin(), m_properties.end(),
                           [&](const MetaProperty& p) { return p.name == name; });
    if (it != m_properties.end()) return &*it;
    return m_base ? m_base->property(name) : nullptr;
}

std::unique_ptr<BaseObject> BaseObject::clone() const {
    return {};
}

JsonArchive::JsonArchive(QJsonObject object, Mode mode)
    : Archive(mode), m_object(std::move(object)) {}

void JsonArchive::value(const QString& name, QVariant& value) {
    if (isWriting()) {
        m_object.insert(name, QJsonValue::fromVariant(value));
    } else {
        value = m_object.value(name).toVariant();
    }
}

void Record::setStream(std::string network, std::string station, std::string location, std::string channel) {
    m_network = std::move(network);
    m_station = std::move(station);
    m_location = std::move(location);
    m_channel = std::move(channel);
}

void Record::setSampling(int count, double frequency) {
    m_sampleCount = count;
    m_samplingFrequency = frequency;
}

std::string Record::streamID() const {
    return m_network + "." + m_station + "." + m_location + "." + m_channel;
}

Time Record::endTime() const {
    if (m_samplingFrequency <= 0.0 || m_sampleCount <= 0) return m_startTime;
    return m_startTime + TimeSpan::seconds(static_cast<double>(m_sampleCount) / m_samplingFrequency);
}

AccelerationRecord::AccelerationRecord(std::vector<double> samples)
    : m_samples(Array::DataType::Double, std::move(samples)) {
    setSampling(m_samples.size(), 0.0);
}

std::unique_ptr<Record> AccelerationRecord::copy() const {
    auto out = std::make_unique<AccelerationRecord>(m_samples.values());
    out->setStream(networkCode(), stationCode(), locationCode(), channelCode());
    out->setStartTime(startTime());
    out->setSampling(sampleCount(), samplingFrequency());
    out->setTimingQuality(timingQuality());
    return out;
}

} // namespace tp::core

namespace tp::core::io {

QByteArray readAll(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

bool writeAll(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(data) == data.size();
}

} // namespace tp::core::io
