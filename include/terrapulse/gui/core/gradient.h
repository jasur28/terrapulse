#pragma once

#include "terrapulse/gui/qt.h"

#include <QColor>

#include <vector>

namespace tp::gui {

struct GradientStop {
    double value = 0.0;
    QColor color;
};

class TP_GUI_API Gradient {
public:
    void add(double value, QColor color);
    QColor colorAt(double value) const;
    const std::vector<GradientStop>& stops() const { return m_stops; }

private:
    std::vector<GradientStop> m_stops;
};

} // namespace tp::gui
