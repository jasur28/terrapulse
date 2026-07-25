#pragma once

#include <QString>

namespace tp::gui {

struct MapsDesc {
    QString location = "https://tile.openstreetmap.org";
    QString type = "osm";
    bool isMercatorProjected = true;
    int minZoom = 0;
    int maxZoom = 19;
    std::size_t cacheSize = 512;
};

} // namespace tp::gui
