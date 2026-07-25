#pragma once

#include "terrapulse/gui/qt.h"

#include <QString>

#include <vector>

namespace tp::gui::plot {

struct LegendItem {
    QString name;
    QString color;
};

class TP_GUI_API AbstractLegend {
public:
    virtual ~AbstractLegend() = default;
    virtual std::vector<LegendItem> items() const = 0;
};

} // namespace tp::gui::plot
