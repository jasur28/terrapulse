#include "controllers/InventoryModel.h"
#include "bus/BusMessage.h"

#include <zmq.hpp>
#include <QDateTime>

InventoryModel::InventoryModel(const std::string& host, tp::master::Queue queue, QObject* parent)
    : QObject(parent)
    , m_sub(tp::master::out(host, queue))
{
    // Backfill the current data model from the master before going live, so the
    // UI is populated immediately instead of waiting for the next messages.
    requestSnapshot(tp::master::ctrl(host), tp::master::queueName(queue));

    m_sub.subscribe("inv.");   // data model updates
    m_sub.subscribe("saf.");   // live per-channel state
    connect(&m_pollTimer, &QTimer::timeout, this, &InventoryModel::poll);
    m_pollTimer.start(250);    // 4 Hz UI refresh is plenty
}

void InventoryModel::requestSnapshot(const std::string& ctrlEndpoint, const std::string& queueName) {
    try {
        zmq::socket_t req(tp::busContext(), zmq::socket_type::req);
        req.set(zmq::sockopt::linger, 0);
        req.set(zmq::sockopt::rcvtimeo, 1500);   // don't hang startup if master is absent
        req.connect(ctrlEndpoint);
        req.send(zmq::buffer(std::string("SNAP ") + queueName), zmq::send_flags::none);

        int applied = 0;
        for (;;) {
            zmq::message_t part;
            auto r = req.recv(part, zmq::recv_flags::none);
            if (!r) break;                       // timeout (no master / no db)
            if (part.size() > 0) {
                QByteArray hb(static_cast<const char*>(part.data()),
                              static_cast<qsizetype>(part.size()));
                const QVariantMap h = tp::BusMessage::decodeHeader(hb);
                if (h.value("type").toString() == "notifier") applyInventory(h);
                else                                          applyFeature(h);
                ++applied;
            }
            if (!part.more()) break;
        }
        if (applied > 0) { rebuild(); emit changed(); }
    } catch (const std::exception&) {
        // best-effort backfill; live subscription will fill in regardless
    }
}

void InventoryModel::poll() {
    bool any = false;
    for (int i = 0; i < 1000; ++i) {
        auto m = m_sub.receive(0);
        if (!m) break;
        const QVariantMap h = tp::BusMessage::decodeHeader(m->header);
        if (m->topic.rfind("inv.", 0) == 0) applyInventory(h);
        else                                applyFeature(h);
        any = true;
    }
    if (any) {
        rebuild();
        emit changed();
    }
}

void InventoryModel::applyInventory(const QVariantMap& h) {
    const QString kind   = h.value("kind").toString();
    const bool    remove = h.value("op").toString() == "remove";
    const quint32 oid    = h.value("objectId").toUInt();

    if (kind == "structure") {
        if (remove) {
            m_structures.remove(oid);
            for (auto it = m_sensors.begin();  it != m_sensors.end();  )
                it = (it.value().value("objectId").toUInt() == oid) ? m_sensors.erase(it)  : ++it;
            for (auto it = m_channels.begin(); it != m_channels.end(); )
                it = (it.value().value("objectId").toUInt() == oid) ? m_channels.erase(it) : ++it;
        } else {
            m_structures[oid] = h;
        }
    } else if (kind == "sensor") {
        const QString key = QString("%1.%2").arg(oid).arg(h.value("sensorId").toUInt());
        if (remove) m_sensors.remove(key); else m_sensors[key] = h;
    } else if (kind == "channel") {
        const QString key = QString("%1.%2.%3").arg(oid)
                                .arg(h.value("sensorId").toUInt())
                                .arg(h.value("component").toInt());
        if (remove) m_channels.remove(key); else m_channels[key] = h;
    }
}

void InventoryModel::applyFeature(const QVariantMap& h) {
    const QString key = QString("%1.%2.%3").arg(h.value("objectId").toUInt())
                            .arg(h.value("sensorId").toUInt())
                            .arg(h.value("component").toInt());
    QVariantMap s;
    s["objectId"]          = h.value("objectId");
    s["sensorId"]          = h.value("sensorId");
    s["health"]            = h.value("healthIndex");
    s["warning"]           = h.value("warningLevel");
    s["rms"]               = h.value("rms");
    s["dominantFrequency"] = h.value("dominantFrequency");
    s["anomalyType"]       = h.value("anomalyType");
    s["ts"]                = h.value("timestamp");
    m_safState[key] = s;
}

void InventoryModel::rebuild() {
    // Aggregate live state per sensor: worst health, worst warning, latest seen.
    QMap<QString, QVariantMap> agg;   // "obj.sen" -> aggregate
    for (const auto& s : m_safState) {
        const QString k = QString("%1.%2").arg(s.value("objectId").toUInt())
                                          .arg(s.value("sensorId").toUInt());
        QVariantMap a = agg.value(k);
        const double h = s.value("health").toDouble();
        const int    w = s.value("warning").toInt();
        if (!a.contains("health") || h < a.value("health").toDouble()) a["health"] = h;
        if (w >= a.value("warning", 0).toInt()) { a["warning"] = w; a["anomalyType"] = s.value("anomalyType"); }
        a["rms"]               = qMax(a.value("rms", 0.0).toDouble(), s.value("rms").toDouble());
        a["dominantFrequency"] = s.value("dominantFrequency");
        const qint64 ts = s.value("ts").toLongLong();
        if (ts > a.value("ts", 0).toLongLong()) a["ts"] = ts;
        agg[k] = a;
    }

    // Per-sensor list (inventory sensors merged with their live aggregate).
    m_sensorList.clear();
    QMap<quint32, QVariantMap> perStruct;   // objectId -> {health,warning}
    for (auto it = m_sensors.constBegin(); it != m_sensors.constEnd(); ++it) {
        QVariantMap sv = it.value();
        const quint32 oid = sv.value("objectId").toUInt();
        const QVariantMap a = agg.value(it.key());
        const bool has = a.contains("health");

        sv["hasData"]           = has;
        sv["health"]            = has ? a.value("health") : QVariant();
        sv["warning"]           = has ? a.value("warning").toInt() : 0;
        sv["rms"]               = has ? a.value("rms") : QVariant();
        sv["dominantFrequency"] = has ? a.value("dominantFrequency") : QVariant();
        sv["lastSeen"]          = has ? QDateTime::fromMSecsSinceEpoch(a.value("ts").toLongLong())
                                            .toString("HH:mm:ss") : QString("—");
        m_sensorList.append(sv);

        if (has) {
            QVariantMap ps = perStruct.value(oid);
            const double h = a.value("health").toDouble();
            const int    w = a.value("warning").toInt();
            if (!ps.contains("health") || h < ps.value("health").toDouble()) ps["health"] = h;
            if (w > ps.value("warning", 0).toInt()) ps["warning"] = w;
            perStruct[oid] = ps;
        }
    }

    QMap<quint32, int> sensorCounts;
    QMap<quint32, int> channelCounts;
    for (const auto& sv : m_sensors)
        ++sensorCounts[sv.value("objectId").toUInt()];
    for (const auto& cv : m_channels)
        ++channelCounts[cv.value("objectId").toUInt()];

    // Per-structure list (inventory + counts + aggregate health/status).
    m_list.clear();
    for (auto it = m_structures.constBegin(); it != m_structures.constEnd(); ++it) {
        const quint32 oid = it.key();
        QVariantMap s = it.value();

        s["sensors"]  = sensorCounts.value(oid);
        s["channels"] = channelCounts.value(oid);

        const QVariantMap ps = perStruct.value(oid);
        s["hasData"] = ps.contains("health");
        s["health"]  = ps.contains("health") ? ps.value("health") : QVariant();
        s["warning"] = ps.value("warning", 0).toInt();
        m_list.append(s);
    }
}
