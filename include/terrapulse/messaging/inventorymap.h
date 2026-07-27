#pragma once

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <string>
#include <unordered_map>

// Station->object map for resolving FDSN source ids on the waveform backbone.
// The stream carries FDSN ids (e.g. "FDSN:UZ_BRDG1_07_H_N_Z") but TerraPulse's
// processing model is keyed by numeric object/sensor. The sensor is the numeric
// location field; only "<network>_<station>" -> objectId needs a lookup, and the
// inventory JSON (the same file tpinv loads) is the source of truth for it.
namespace tp {

// Build "<network>_<station>" -> objectId from an inventory JSON file. Returns an
// empty map if the file is missing/unreadable (then only legacy numeric ids
// resolve, which is the pre-Level-2 behaviour).
inline std::unordered_map<std::string, uint32_t> loadStationMap(const QString& path) {
    std::unordered_map<std::string, uint32_t> map;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return map;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonArray structures = doc.object().value("structures").toArray();
    for (const QJsonValue& sv : structures) {
        const QJsonObject s = sv.toObject();
        const QString net = s.value("network").toString();
        const QString sta = s.value("stationCode").toString();
        if (net.isEmpty() || sta.isEmpty()) continue;
        map[(net + "_" + sta).toStdString()] =
            static_cast<uint32_t>(s.value("objectId").toInt());
    }
    return map;
}

} // namespace tp
