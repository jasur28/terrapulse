#include "terrapulse/gui/core/application.h"
#include "terrapulse/gui/core/connectionstatelabel.h"
#include "terrapulse/gui/core/logmanager.h"
#include "terrapulse/gui/core/processmanager.h"
#include "terrapulse/gui/core/recordpolyline.h"
#include "terrapulse/gui/core/recordstreamthread.h"
#include "terrapulse/gui/core/recordview.h"
#include "terrapulse/gui/core/timescale.h"
#include "terrapulse/gui/core/utils.h"
#include "terrapulse/gui/datamodel/eventlistview.h"
#include "terrapulse/gui/datamodel/eventdatarepository.h"
#include "terrapulse/gui/datamodel/inventorylistview.h"
#include "terrapulse/gui/datamodel/stationdata.h"
#include "terrapulse/gui/datamodel/stationsymbol.h"
#include "terrapulse/gui/plot.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tp::gui {

ApplicationContext::ApplicationContext(QString moduleName, int flags)
    : m_moduleName(std::move(moduleName)), m_flags(flags) {}

void Gradient::add(double value, QColor color) {
    m_stops.push_back({value, std::move(color)});
    std::sort(m_stops.begin(), m_stops.end(), [](const auto& a, const auto& b) {
        return a.value < b.value;
    });
}

QColor Gradient::colorAt(double value) const {
    if (m_stops.empty()) {
        return {};
    }
    if (value <= m_stops.front().value) {
        return m_stops.front().color;
    }
    if (value >= m_stops.back().value) {
        return m_stops.back().color;
    }
    for (std::size_t i = 1; i < m_stops.size(); ++i) {
        if (value <= m_stops[i].value) {
            return m_stops[i].color;
        }
    }
    return m_stops.back().color;
}

void ConnectionStateLabel::setState(ConnectionState value) {
    state = value;
    switch (state) {
    case ConnectionState::Offline:
        text = "Offline";
        color = QColor(110, 113, 121);
        break;
    case ConnectionState::Connecting:
        text = "Connecting";
        color = QColor(255, 205, 33);
        break;
    case ConnectionState::Connected:
        text = "Connected";
        color = QColor(20, 199, 114);
        break;
    case ConnectionState::Error:
        text = "Error";
        color = QColor(255, 44, 85);
        break;
    }
}

void LogManager::append(QString level, QString module, QString message) {
    m_entries.push_back({std::move(level), std::move(module), std::move(message)});
    trim();
}

void LogManager::trim() {
    while (m_entries.size() > m_capacity) {
        m_entries.pop_front();
    }
}

ModuleProcess* ProcessManager::module(const QString& name) {
    for (auto& module : m_modules) {
        if (module.name == name) {
            return &module;
        }
    }
    return nullptr;
}

double TimeScale::xOf(const tp::core::Time& time, double width) const {
    const auto length = m_window.length().microseconds();
    if (length <= 0 || width <= 0) {
        return 0.0;
    }
    return static_cast<double>((time - m_window.startTime()).microseconds()) / static_cast<double>(length) * width;
}

tp::core::Time TimeScale::timeAt(double x, double width) const {
    if (width <= 0) {
        return m_window.startTime();
    }
    const auto us = static_cast<std::int64_t>(m_window.length().microseconds() * (x / width));
    return m_window.startTime() + tp::core::TimeSpan(std::chrono::microseconds(us));
}

RecordViewItem* RecordViewModel::addItem(tp::datamodel::WaveformStreamID streamID, QString label) {
    if (item(streamID.streamID())) {
        return nullptr;
    }
    auto item = std::make_unique<RecordViewItem>();
    item->streamID = std::move(streamID);
    item->label = std::move(label);
    auto* result = item.get();
    m_items.push_back(std::move(item));
    return result;
}

bool RecordViewModel::addRecord(std::shared_ptr<tp::core::Record> record) {
    if (!record) {
        return false;
    }
    auto* target = item(QString::fromStdString(record->streamID()));
    if (!target) {
        tp::datamodel::WaveformStreamID streamID;
        streamID.networkCode = QString::fromStdString(record->networkCode());
        streamID.stationCode = QString::fromStdString(record->stationCode());
        streamID.locationCode = QString::fromStdString(record->locationCode());
        streamID.channelCode = QString::fromStdString(record->channelCode());
        target = addItem(streamID, QString::fromStdString(record->stationCode()));
    }
    target->records.push_back(std::move(record));
    return true;
}

void RecordViewModel::clear() {
    m_items.clear();
}

void RecordViewModel::clearRecords() {
    for (auto& item : m_items) {
        item->records.clear();
    }
}

RecordViewItem* RecordViewModel::itemAt(int row) const {
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
        return nullptr;
    }
    return m_items[static_cast<std::size_t>(row)].get();
}

RecordViewItem* RecordViewModel::item(const QString& streamID) const {
    for (const auto& item : m_items) {
        if (item && item->streamID.streamID() == streamID) {
            return item.get();
        }
    }
    return nullptr;
}

std::vector<QPointF> buildRecordPolyline(const tp::core::Record& record, double width, double height) {
    std::vector<QPointF> points;
    const auto* array = dynamic_cast<const tp::core::DoubleArray*>(record.data());
    if (!array || array->values().empty() || width <= 0 || height <= 0) {
        return points;
    }
    const auto& values = array->values();
    auto minmax = std::minmax_element(values.begin(), values.end());
    const double minValue = *minmax.first;
    const double maxValue = *minmax.second;
    const double span = std::max(1e-12, maxValue - minValue);
    points.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double x = values.size() > 1 ? static_cast<double>(i) / (values.size() - 1) * width : 0.0;
        const double y = height - ((values[i] - minValue) / span * height);
        points.emplace_back(x, y);
    }
    return points;
}

RecordStreamThread::RecordStreamThread(QString recordStreamURL, QObject* parent)
    : QThread(parent), m_recordStreamURL(std::move(recordStreamURL)) {}

RecordStreamThread::~RecordStreamThread() {
    stop(true);
}

void RecordStreamThread::stop(bool waitForTermination) {
    m_requestedClose = true;
    requestInterruption();
    if (waitForTermination && isRunning()) {
        wait();
    }
}

void RecordStreamThread::run() {
    try {
        auto stream = tp::io::RecordStream::Open(m_recordStreamURL);
        if (!stream) {
            emit handleError(QString("Unable to open record stream: %1").arg(m_recordStreamURL));
            return;
        }
        while (!m_requestedClose && !isInterruptionRequested()) {
            auto record = stream->next();
            if (!record) {
                break;
            }
            emit receivedRecord(std::shared_ptr<tp::core::Record>(std::move(record)));
        }
    }
    catch (const std::exception& e) {
        emit handleError(QString::fromLocal8Bit(e.what()));
    }
}

QString severityText(int severity) {
    switch (severity) {
    case 0: return "Low";
    case 1: return "Medium";
    case 2: return "High";
    case 3: return "Critical";
    default: return "Unknown";
    }
}

QString connectionText(bool connected) {
    return connected ? "Connected" : "Offline";
}

namespace datamodel {

StationSymbol symbolForStructure(const tp::datamodel::Structure& structure, QColor color) {
    StationSymbol symbol;
    symbol.publicID = structure.publicID();
    symbol.label = structure.name.isEmpty() ? structure.code : structure.name;
    symbol.latitude = structure.latitude;
    symbol.longitude = structure.longitude;
    symbol.color = std::move(color);
    return symbol;
}

EventSummary summarize(const tp::datamodel::Event& event) {
    EventSummary summary;
    summary.publicID = event.publicID();
    summary.structureID = event.structureID;
    summary.time = event.time.toString();
    summary.type = QString::number(static_cast<int>(event.type));
    summary.severity = tp::gui::severityText(static_cast<int>(event.severity));
    summary.status = QString::number(static_cast<int>(event.status));
    return summary;
}

EventListModel::EventListModel(QObject* parent) : QAbstractListModel(parent) {}

int EventListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_events.size());
}

QVariant EventListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto& event = m_events[static_cast<std::size_t>(index.row())];
    if (!event) {
        return {};
    }
    switch (role) {
    case PublicIDRole: return event->publicID();
    case StructureIDRole: return event->structureID;
    case TimeRole: return event->time.toString();
    case TypeRole: return static_cast<int>(event->type);
    case SeverityRole: return static_cast<int>(event->severity);
    case StatusRole: return static_cast<int>(event->status);
    default: return {};
    }
}

QHash<int, QByteArray> EventListModel::roleNames() const {
    return {
        {PublicIDRole, "publicID"},
        {StructureIDRole, "structureID"},
        {TimeRole, "time"},
        {TypeRole, "type"},
        {SeverityRole, "severity"},
        {StatusRole, "status"}
    };
}

void EventListModel::setEvents(std::vector<std::shared_ptr<tp::datamodel::Event>> events) {
    beginResetModel();
    m_events = std::move(events);
    endResetModel();
}

void EventListModel::clear() {
    beginResetModel();
    m_events.clear();
    endResetModel();
}

InventoryListModel::InventoryListModel(QObject* parent) : QAbstractListModel(parent) {}

int InventoryListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_structures.size());
}

QVariant InventoryListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto& structure = m_structures[static_cast<std::size_t>(index.row())];
    if (!structure) {
        return {};
    }
    switch (role) {
    case PublicIDRole: return structure->publicID();
    case CodeRole: return structure->code;
    case NameRole: return structure->name;
    case TypeRole: return structure->type;
    case LatitudeRole: return structure->latitude;
    case LongitudeRole: return structure->longitude;
    case SensorCountRole: return static_cast<int>(structure->sensors.size());
    default: return {};
    }
}

QHash<int, QByteArray> InventoryListModel::roleNames() const {
    return {
        {PublicIDRole, "publicID"},
        {CodeRole, "code"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {LatitudeRole, "latitude"},
        {LongitudeRole, "longitude"},
        {SensorCountRole, "sensorCount"}
    };
}

void InventoryListModel::setInventory(const tp::datamodel::Inventory& inventory) {
    beginResetModel();
    m_structures = inventory.structures();
    endResetModel();
}

void InventoryListModel::clear() {
    beginResetModel();
    m_structures.clear();
    endResetModel();
}

void StationData::resetTransientState() {
    triggering = false;
    associated = false;
    selected = false;
    groundMotion = 0.0;
    groundMotionColor = Qt::black;
    triggerAmplitude = 0.0;
    triggerLifeSpan = tp::core::TimeSpan::seconds(0.0);
    quality.clear();
    qualityStatus = StationQualityStatus::NotSet;
    qualityColor = Qt::black;
}

StationData* StationDataCollection::add(StationData data) {
    if (auto* existing = find(data.id)) {
        *existing = std::move(data);
        return existing;
    }
    m_data.push_back(std::move(data));
    return &m_data.back();
}

bool StationDataCollection::remove(const QString& id) {
    const auto it = std::remove_if(m_data.begin(), m_data.end(), [&id](const StationData& station) {
        return station.id == id;
    });
    if (it == m_data.end()) {
        return false;
    }
    m_data.erase(it, m_data.end());
    return true;
}

StationData* StationDataCollection::find(const QString& id) {
    const auto it = std::find_if(m_data.begin(), m_data.end(), [&id](const StationData& station) {
        return station.id == id;
    });
    return it == m_data.end() ? nullptr : &*it;
}

const StationData* StationDataCollection::find(const QString& id) const {
    const auto it = std::find_if(m_data.begin(), m_data.end(), [&id](const StationData& station) {
        return station.id == id;
    });
    return it == m_data.end() ? nullptr : &*it;
}

void StationDataCollection::clear() {
    m_data.clear();
}

void GroundMotionHandler::handle(StationData& station, const tp::core::Record& record) {
    const auto* samples = dynamic_cast<const tp::core::DoubleArray*>(record.data());
    if (!samples || samples->values().empty()) {
        return;
    }

    double maxAbs = 0.0;
    for (double value : samples->values()) {
        maxAbs = std::max(maxAbs, std::abs(value * station.groundMotionGain));
    }

    station.groundMotion = maxAbs;
    station.lastRecordTime = tp::core::Time::now();
    station.lastSampleTime = record.endTime();
    station.groundMotionColor = colorForGroundMotion(maxAbs);
}

void GroundMotionHandler::update(StationData& station) {
    const auto age = tp::core::Time::now() - station.lastRecordTime;
    if (age.microseconds() > m_recordLifeSpan.microseconds()) {
        station.groundMotion = 0.0;
        station.groundMotionColor = QColor(95, 98, 105);
    }
}

QColor GroundMotionHandler::colorForGroundMotion(double value) const {
    if (value <= 0.0) return QColor(34, 34, 34);
    if (value < 0.02) return QColor(22, 175, 84);
    if (value < 0.05) return QColor(198, 228, 34);
    if (value < 0.10) return QColor(255, 218, 41);
    if (value < 0.25) return QColor(255, 150, 30);
    return QColor(224, 35, 35);
}

void TriggerHandler::handlePick(StationData& station, double amplitude, tp::core::Time time) {
    station.triggering = true;
    station.triggerAmplitude = std::abs(amplitude);
    station.triggerTime = time;
    station.triggerLifeSpan = m_pickLifeSpan;
}

void TriggerHandler::update(StationData& station) {
    if (!station.triggering) {
        return;
    }
    const auto age = tp::core::Time::now() - station.triggerTime;
    if (age.microseconds() > station.triggerLifeSpan.microseconds()) {
        station.triggering = false;
        station.triggerAmplitude = 0.0;
        station.triggerLifeSpan = tp::core::TimeSpan::seconds(0.0);
    }
}

void QualityHandler::handle(StationData& station, const tp::datamodel::WaveformQuality& quality) {
    QualityValue value;
    value.value = quality.value;
    value.lowerUncertainty = quality.lowerUncertainty;
    value.upperUncertainty = quality.upperUncertainty;
    value.status = statusFor(quality.parameter.isEmpty() ? quality.type : quality.parameter, quality.value);
    station.quality.insert(quality.parameter.isEmpty() ? quality.type : quality.parameter, value);
    update(station);
}

void QualityHandler::update(StationData& station) {
    StationQualityStatus worst = StationQualityStatus::NotSet;
    for (auto it = station.quality.constBegin(); it != station.quality.constEnd(); ++it) {
        if (static_cast<int>(it.value().status) > static_cast<int>(worst)) {
            worst = it.value().status;
        }
    }
    station.qualityStatus = worst;
    station.qualityColor = colorFor(worst);
}

StationQualityStatus QualityHandler::statusFor(const QString& parameter, double value) const {
    const QString key = parameter.toLower();
    if (key.contains("availability")) {
        if (value < 80.0) return StationQualityStatus::Error;
        if (value < 95.0) return StationQualityStatus::Warning;
        return StationQualityStatus::Ok;
    }
    if (key.contains("gap") || key.contains("overlap") || key.contains("spike")) {
        if (value > 10.0) return StationQualityStatus::Error;
        if (value > 0.0) return StationQualityStatus::Warning;
        return StationQualityStatus::Ok;
    }
    if (key.contains("delay") || key.contains("latency")) {
        if (value > 180.0) return StationQualityStatus::Error;
        if (value > 60.0) return StationQualityStatus::Warning;
        return StationQualityStatus::Ok;
    }
    if (!std::isfinite(value)) {
        return StationQualityStatus::NotSet;
    }
    return StationQualityStatus::Ok;
}

QColor QualityHandler::colorFor(StationQualityStatus status) const {
    switch (status) {
    case StationQualityStatus::Ok: return QColor(22, 175, 84);
    case StationQualityStatus::Warning: return QColor(255, 205, 33);
    case StationQualityStatus::Error: return QColor(255, 59, 48);
    case StationQualityStatus::NotSet: return QColor(95, 98, 105);
    }
    return QColor(95, 98, 105);
}

StationDataModel::StationDataModel(QObject* parent) : QAbstractListModel(parent) {}

int StationDataModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_stations.size();
}

QVariant StationDataModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const StationData& station = m_stations.values()[static_cast<std::size_t>(index.row())];
    switch (role) {
    case IDRole: return station.id;
    case StructureIDRole: return station.structureID;
    case StructureCodeRole: return station.structureCode;
    case SensorIDRole: return station.sensorID;
    case SensorCodeRole: return station.sensorCode;
    case DisplayCodeRole: return station.displayCode;
    case LatitudeRole: return station.latitude;
    case LongitudeRole: return station.longitude;
    case ElevationRole: return station.elevation;
    case EnabledRole: return station.enabled;
    case TriggeringRole: return station.triggering;
    case AssociatedRole: return station.associated;
    case SelectedRole: return station.selected;
    case GroundMotionRole: return station.groundMotion;
    case GroundMotionColorRole: return station.groundMotionColor;
    case QualityStatusRole: return static_cast<int>(station.qualityStatus);
    case QualityColorRole: return station.qualityColor;
    case LastRecordTimeRole: return station.lastRecordTime.toString();
    default: return {};
    }
}

QHash<int, QByteArray> StationDataModel::roleNames() const {
    return {
        {IDRole, "id"},
        {StructureIDRole, "structureID"},
        {StructureCodeRole, "structureCode"},
        {SensorIDRole, "sensorID"},
        {SensorCodeRole, "sensorCode"},
        {DisplayCodeRole, "displayCode"},
        {LatitudeRole, "latitude"},
        {LongitudeRole, "longitude"},
        {ElevationRole, "elevation"},
        {EnabledRole, "enabled"},
        {TriggeringRole, "triggering"},
        {AssociatedRole, "associated"},
        {SelectedRole, "selected"},
        {GroundMotionRole, "groundMotion"},
        {GroundMotionColorRole, "groundMotionColor"},
        {QualityStatusRole, "qualityStatus"},
        {QualityColorRole, "qualityColor"},
        {LastRecordTimeRole, "lastRecordTime"}
    };
}

void StationDataModel::setStations(std::vector<StationData> stations) {
    beginResetModel();
    m_stations.clear();
    for (auto& station : stations) {
        m_stations.add(std::move(station));
    }
    endResetModel();
}

void StationDataModel::setInventory(const tp::datamodel::Inventory& inventory) {
    std::vector<StationData> stations;
    for (const auto& structure : inventory.structures()) {
        if (!structure) {
            continue;
        }
        if (structure->sensors.empty()) {
            StationData station;
            station.id = structure->publicID();
            station.structureID = structure->publicID();
            station.structureCode = structure->code;
            station.displayCode = structure->code.isEmpty() ? structure->name : structure->code;
            station.latitude = structure->latitude;
            station.longitude = structure->longitude;
            stations.push_back(std::move(station));
            continue;
        }
        for (const auto& sensor : structure->sensors) {
            if (!sensor) {
                continue;
            }
            StationData station;
            station.id = sensor->publicID().isEmpty()
                ? QStringLiteral("%1.%2").arg(structure->publicID(), sensor->code)
                : sensor->publicID();
            station.structureID = structure->publicID();
            station.structureCode = structure->code;
            station.sensorID = sensor->publicID();
            station.sensorCode = sensor->code;
            station.displayCode = QStringLiteral("%1.%2").arg(structure->code, sensor->code);
            station.latitude = sensor->latitude == 0.0 ? structure->latitude : sensor->latitude;
            station.longitude = sensor->longitude == 0.0 ? structure->longitude : sensor->longitude;
            station.elevation = sensor->elevation;
            stations.push_back(std::move(station));
        }
    }
    setStations(std::move(stations));
}

StationData* StationDataModel::station(const QString& id) {
    return m_stations.find(id);
}

void StationDataModel::notifyStationChanged(const QString& id) {
    const auto& values = m_stations.values();
    for (int row = 0; row < static_cast<int>(values.size()); ++row) {
        if (values[static_cast<std::size_t>(row)].id == id) {
            const QModelIndex modelIndex = index(row, 0);
            emit dataChanged(modelIndex, modelIndex);
            return;
        }
    }
}

void StationDataModel::clear() {
    beginResetModel();
    m_stations.clear();
    endResetModel();
}

EventData* EventDataRepository::addEvent(std::shared_ptr<tp::datamodel::Event> event, bool visible) {
    if (!event) {
        return nullptr;
    }
    if (auto* existing = findEvent(event->publicID())) {
        existing->event = std::move(event);
        existing->visible = visible;
        existing->containerCreationTime = tp::core::Time::now();
        return existing;
    }
    EventData data;
    data.event = std::move(event);
    data.visible = visible;
    m_events.push_back(std::move(data));
    return &m_events.back();
}

bool EventDataRepository::removeEvent(const QString& id) {
    const auto it = std::remove_if(m_events.begin(), m_events.end(), [&id](const EventData& eventData) {
        return eventData.id() == id;
    });
    if (it == m_events.end()) {
        return false;
    }
    m_events.erase(it, m_events.end());
    return true;
}

EventData* EventDataRepository::findEvent(const QString& id) {
    const auto it = std::find_if(m_events.begin(), m_events.end(), [&id](const EventData& eventData) {
        return eventData.id() == id;
    });
    return it == m_events.end() ? nullptr : &*it;
}

const EventData* EventDataRepository::findEvent(const QString& id) const {
    const auto it = std::find_if(m_events.begin(), m_events.end(), [&id](const EventData& eventData) {
        return eventData.id() == id;
    });
    return it == m_events.end() ? nullptr : &*it;
}

EventData* EventDataRepository::latestEvent() {
    if (m_events.empty()) {
        return nullptr;
    }
    auto it = std::max_element(m_events.begin(), m_events.end(), [](const EventData& a, const EventData& b) {
        if (!a.event) return true;
        if (!b.event) return false;
        return a.event->time < b.event->time;
    });
    return it == m_events.end() ? nullptr : &*it;
}

EventData* EventDataRepository::findNextExpiredEvent(tp::core::Time now) {
    const auto it = std::find_if(m_events.begin(), m_events.end(), [this, &now](const EventData& eventData) {
        return (now - eventData.containerCreationTime).microseconds() > m_eventLifeSpan.microseconds();
    });
    return it == m_events.end() ? nullptr : &*it;
}

void EventDataRepository::clear() {
    m_events.clear();
}

std::vector<std::shared_ptr<tp::datamodel::Event>> EventDataRepository::events() const {
    std::vector<std::shared_ptr<tp::datamodel::Event>> result;
    result.reserve(m_events.size());
    for (const auto& eventData : m_events) {
        if (eventData.event && eventData.visible) {
            result.push_back(eventData.event);
        }
    }
    return result;
}

} // namespace datamodel

namespace plot {

void Range::reset() {
    lower = 0.0;
    upper = 0.0;
    valid = false;
}

void Range::normalize() {
    if (valid && lower > upper) {
        std::swap(lower, upper);
    }
}

void Range::expand(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    if (!valid) {
        lower = upper = value;
        valid = true;
        return;
    }
    lower = std::min(lower, value);
    upper = std::max(upper, value);
}

void Range::merge(const Range& other) {
    if (!other.valid) {
        return;
    }
    expand(other.lower);
    expand(other.upper);
}

bool Range::contains(double value) const {
    return valid && value >= lower && value <= upper;
}

Range Range::padded(double ratio) const {
    if (!valid) {
        return {};
    }
    const double pad = std::max(length() * ratio, 1e-9);
    return {lower - pad, upper + pad};
}

void Axis::setRange(Range range) {
    range.normalize();
    m_range = range;
}

void Axis::extendRange(const Range& range) {
    m_range.merge(range);
}

double Axis::project(double pixel, double pixelMin, double pixelMax) const {
    if (!m_range.isValid() || pixelMax == pixelMin) {
        return m_range.lower;
    }
    return m_range.lower + (pixel - pixelMin) / (pixelMax - pixelMin) * m_range.length();
}

double Axis::unproject(double value, double pixelMin, double pixelMax) const {
    if (!m_range.isValid() || m_range.length() == 0.0) {
        return pixelMin;
    }
    return pixelMin + (value - m_range.lower) / m_range.length() * (pixelMax - pixelMin);
}

std::vector<Axis::Tick> Axis::ticks(int maxTicks, double pixelMin, double pixelMax) const {
    std::vector<Tick> result;
    if (!m_range.isValid() || maxTicks <= 0) {
        return result;
    }
    const int count = std::max(2, maxTicks);
    result.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const double t = count == 1 ? 0.0 : static_cast<double>(i) / (count - 1);
        const double value = m_range.lower + t * m_range.length();
        result.push_back({value, unproject(value, pixelMin, pixelMax), QString::number(value, 'g', 4)});
    }
    return result;
}

Range DataXY::xRange() const {
    Range range;
    for (const auto& point : data) {
        range.expand(point.x());
    }
    return range;
}

Range DataXY::yRange() const {
    Range range;
    for (const auto& point : data) {
        range.expand(point.y());
    }
    return range;
}

QPolygonF DataXY::project(const Axis& xAxis, const Axis& yAxis, double width, double height) const {
    QPolygonF poly;
    poly.reserve(data.size());
    for (const auto& point : data) {
        poly.append({xAxis.unproject(point.x(), 0, width), yAxis.unproject(point.y(), height, 0)});
    }
    return poly;
}

Range DataY::yRange() const {
    Range range;
    for (const auto value : y) {
        range.expand(value);
    }
    return range;
}

QPolygonF DataY::project(const Axis& xAxis, const Axis& yAxis, double width, double height) const {
    QPolygonF poly;
    if (y.isEmpty()) {
        return poly;
    }
    poly.reserve(y.size());
    const double start = x.isValid() ? x.lower : 0.0;
    const double step = y.size() > 1 && x.isValid() ? x.length() / (y.size() - 1) : 1.0;
    for (int i = 0; i < y.size(); ++i) {
        const double key = start + step * i;
        poly.append({xAxis.unproject(key, 0, width), yAxis.unproject(y[i], height, 0)});
    }
    return poly;
}

Graph::Graph(QString name) : m_name(std::move(name)), m_pen(QColor(20, 199, 114)) {}

void Graph::setColor(QColor color) {
    m_pen.setColor(std::move(color));
}

QPolygonF Graph::project(const Axis& xAxis, const Axis& yAxis, double width, double height) const {
    if (!m_data || !visible) {
        return {};
    }
    return m_data->project(xAxis, yAxis, width, height);
}

Graph* Plot::addGraph(QString name) {
    auto graph = std::make_unique<Graph>(std::move(name));
    auto* result = graph.get();
    m_graphs.push_back(std::move(graph));
    return result;
}

void Plot::clearGraphs() {
    m_graphs.clear();
}

void Plot::updateRanges() {
    Range x;
    Range y;
    for (const auto& graph : m_graphs) {
        if (!graph || graph->isEmpty()) {
            continue;
        }
        x.merge(graph->xRange());
        y.merge(graph->yRange());
    }
    xAxis.setRange(x.padded(0.02));
    yAxis.setRange(y.padded(0.08));
}

std::vector<QPolygonF> Plot::project(double width, double height) const {
    std::vector<QPolygonF> result;
    result.reserve(m_graphs.size());
    for (const auto& graph : m_graphs) {
        if (graph && graph->visible) {
            result.push_back(graph->project(xAxis, yAxis, width, height));
        }
    }
    return result;
}

} // namespace plot
} // namespace tp::gui
