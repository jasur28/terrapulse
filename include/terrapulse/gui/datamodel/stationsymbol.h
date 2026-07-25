#pragma once

#include "terrapulse/datamodel/inventory.h"

#include <QColor>
#include <QString>

namespace tp::gui::datamodel {

struct StationSymbol {
    QString publicID;
    QString label;
    double latitude = 0.0;
    double longitude = 0.0;
    QColor color{20, 199, 114};
    QColor outlineColor{8, 12, 22};
    int radius = 7;
    bool selected = false;
};

StationSymbol symbolForStructure(const tp::datamodel::Structure& structure, QColor color);

} // namespace tp::gui::datamodel
