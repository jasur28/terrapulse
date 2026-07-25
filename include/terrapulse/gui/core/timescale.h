#pragma once

#include "terrapulse/core/timewindow.h"
#include "terrapulse/gui/qt.h"

namespace tp::gui {

class TP_GUI_API TimeScale {
public:
    void setTimeWindow(tp::core::TimeWindow window) { m_window = window; }
    tp::core::TimeWindow timeWindow() const { return m_window; }

    double xOf(const tp::core::Time& time, double width) const;
    tp::core::Time timeAt(double x, double width) const;

private:
    tp::core::TimeWindow m_window;
};

} // namespace tp::gui
