#pragma once

#include "terrapulse/core/record.h"
#include "terrapulse/datamodel/inventory.h"
#include "terrapulse/datamodel/waveformquality.h"
#include "terrapulse/gui/qt.h"

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QString>

#include <memory>
#include <vector>

namespace tp::gui::datamodel {

enum class StationRuntimeMode {
    Network,
    GroundMotion,
    QualityControl,
    Events
};

enum class StationQualityStatus {
    NotSet,
    Ok,
    Warning,
    Error
};

struct TP_GUI_API QualityValue {
    double value = 0.0;
    double lowerUncertainty = 0.0;
    double upperUncertainty = 0.0;
    StationQualityStatus status = StationQualityStatus::NotSet;
};

struct TP_GUI_API StationData {
    QString id;
    QString structureID;
    QString structureCode;
    QString sensorID;
    QString sensorCode;
    QString displayCode;

    double latitude = 0.0;
    double longitude = 0.0;
    double elevation = 0.0;

    bool enabled = true;
    bool triggering = false;
    bool associated = false;
    bool selected = false;

    double groundMotion = 0.0;
    double groundMotionGain = 1.0;
    QColor groundMotionColor = Qt::black;
    tp::core::Time lastRecordTime;
    tp::core::Time lastSampleTime;

    double triggerAmplitude = 0.0;
    tp::core::Time triggerTime;
    tp::core::TimeSpan triggerLifeSpan = tp::core::TimeSpan::seconds(0.0);

    QHash<QString, QualityValue> quality;
    StationQualityStatus qualityStatus = StationQualityStatus::NotSet;
    QColor qualityColor = Qt::black;

    void resetTransientState();
};

class TP_GUI_API StationDataCollection {
public:
    using Container = std::vector<StationData>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    StationData* add(StationData data);
    bool remove(const QString& id);
    StationData* find(const QString& id);
    const StationData* find(const QString& id) const;
    void clear();

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    int size() const { return static_cast<int>(m_data.size()); }
    const Container& values() const { return m_data; }

private:
    Container m_data;
};

class TP_GUI_API StationDataHandler {
public:
    virtual ~StationDataHandler() = default;
    virtual void update(StationData& station) = 0;
};

class TP_GUI_API GroundMotionHandler : public StationDataHandler {
public:
    void handle(StationData& station, const tp::core::Record& record);
    void update(StationData& station) override;
    void setRecordLifeSpan(tp::core::TimeSpan span) { m_recordLifeSpan = span; }

private:
    QColor colorForGroundMotion(double value) const;
    tp::core::TimeSpan m_recordLifeSpan = tp::core::TimeSpan::seconds(30.0);
};

class TP_GUI_API TriggerHandler : public StationDataHandler {
public:
    void handlePick(StationData& station, double amplitude, tp::core::Time time);
    void update(StationData& station) override;
    void setPickLifeSpan(tp::core::TimeSpan span) { m_pickLifeSpan = span; }

private:
    tp::core::TimeSpan m_pickLifeSpan = tp::core::TimeSpan::seconds(90.0);
};

class TP_GUI_API QualityHandler : public StationDataHandler {
public:
    void handle(StationData& station, const tp::datamodel::WaveformQuality& quality);
    void update(StationData& station) override;

private:
    StationQualityStatus statusFor(const QString& parameter, double value) const;
    QColor colorFor(StationQualityStatus status) const;
};

class TP_GUI_API StationDataModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IDRole = Qt::UserRole + 1,
        StructureIDRole,
        StructureCodeRole,
        SensorIDRole,
        SensorCodeRole,
        DisplayCodeRole,
        LatitudeRole,
        LongitudeRole,
        ElevationRole,
        EnabledRole,
        TriggeringRole,
        AssociatedRole,
        SelectedRole,
        GroundMotionRole,
        GroundMotionColorRole,
        QualityStatusRole,
        QualityColorRole,
        LastRecordTimeRole
    };

    explicit StationDataModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setStations(std::vector<StationData> stations);
    void setInventory(const tp::datamodel::Inventory& inventory);
    StationData* station(const QString& id);
    const StationDataCollection& collection() const { return m_stations; }
    void notifyStationChanged(const QString& id);
    void clear();

private:
    StationDataCollection m_stations;
};

} // namespace tp::gui::datamodel
