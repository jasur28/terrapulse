#pragma once
#include "bus/Bus.h"
#include "bus/Master.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QString>
#include <string>

// InventoryModel — the client-side copy of the data model. It maintains:
//   * the inventory tree (Structure -> Sensor -> Channel), applied from INVENTORY
//     notifiers (inv., add/update/remove);
//   * the latest live state per channel, from FEATURE messages (saf.),
// and merges them into per-sensor and per-structure aggregates for the UI
// (health = worst channel, status = worst warning). This is the shared model
// every GUI page reads from.
class InventoryModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList structures     READ structures     NOTIFY changed)
    Q_PROPERTY(QVariantList sensors        READ sensors        NOTIFY changed)
    Q_PROPERTY(int          structureCount READ structureCount NOTIFY changed)
    Q_PROPERTY(int          sensorCount    READ sensorCount    NOTIFY changed)

public:
    // host = tpmaster host; queue selects production (live) or playback (review).
    // Subscribes on that queue's output and backfills via the control port.
    explicit InventoryModel(const std::string& host,
                            tp::master::Queue queue = tp::master::Queue::Production,
                            QObject* parent = nullptr);

    QVariantList structures()     const { return m_list; }
    QVariantList sensors()        const { return m_sensorList; }
    int          structureCount() const { return int(m_structures.size()); }
    int          sensorCount()    const { return int(m_sensors.size()); }

signals:
    void changed();

private slots:
    void poll();

private:
    void requestSnapshot(const std::string& ctrlEndpoint, const std::string& queueName);  // REQ/REP backfill
    void applyInventory(const QVariantMap& h);
    void applyFeature(const QVariantMap& h);
    void rebuild();

    tp::Subscriber m_sub;
    QTimer         m_pollTimer;

    // Inventory (static).
    QMap<quint32, QVariantMap> m_structures;   // objectId       -> fields
    QMap<QString, QVariantMap> m_sensors;      // "obj.sen"       -> fields
    QMap<QString, QVariantMap> m_channels;     // "obj.sen.comp"  -> fields
    // Live state (from saf.).
    QMap<QString, QVariantMap> m_safState;     // "obj.sen.comp"  -> latest feature

    QVariantList m_list;        // structures (+ counts + aggregate health/status)
    QVariantList m_sensorList;  // sensors    (+ live health/status/rms/last-seen)
};
