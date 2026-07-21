#pragma once
#include <QString>
#include <QVariantMap>

// TerraPulse inventory data model: the fixed description of WHAT is monitored.
//   Structure  — a monitored object (bridge, building, dam ...)   [object_id]
//     Sensor   — one accelerometer node on that structure          [sensor_id]
//       Channel — one measured axis of that sensor (X/Y/Z)         [component]
//
// Objects are distributed as notifiers (add/update/remove) on the INVENTORY
// group and persisted by tpmaster's dbstore. `key()` is the object's stable
// identity used in the notifier topic `inv.<kind>.<key>`.
namespace tp::inv {

struct Structure {
    quint32 objectId = 0;
    QString name;
    double  lat = 0.0, lon = 0.0;
    QString description;

    QString key() const { return QString::number(objectId); }
    QVariantMap toVariant() const {
        return { {"kind","structure"}, {"objectId",objectId}, {"name",name},
                 {"lat",lat}, {"lon",lon}, {"description",description} };
    }
};

struct Sensor {
    quint32 objectId = 0, sensorId = 0;
    QString model, location;

    QString key() const { return QString("%1.%2").arg(objectId).arg(sensorId); }
    QVariantMap toVariant() const {
        return { {"kind","sensor"}, {"objectId",objectId}, {"sensorId",sensorId},
                 {"model",model}, {"location",location} };
    }
};

struct Channel {
    quint32 objectId = 0, sensorId = 0;
    int     component = 0;          // X=0, Y=1, Z=2
    quint32 sampleRate = 0;
    QString unit;                   // "gal" | "mg" | "m/s2"
    double  gain = 1.0;

    QString key() const {
        return QString("%1.%2.%3").arg(objectId).arg(sensorId).arg(component);
    }
    QVariantMap toVariant() const {
        return { {"kind","channel"}, {"objectId",objectId}, {"sensorId",sensorId},
                 {"component",component}, {"sampleRate",sampleRate},
                 {"unit",unit}, {"gain",gain} };
    }
};

} // namespace tp::inv
