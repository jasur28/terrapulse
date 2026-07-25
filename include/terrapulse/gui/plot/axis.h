#pragma once

#include "terrapulse/gui/plot/range.h"
#include "terrapulse/gui/qt.h"

#include <QString>

#include <vector>

namespace tp::gui::plot {

class TP_GUI_API Axis {
public:
    enum class Position {
        Left,
        Right,
        Top,
        Bottom
    };

    struct Tick {
        double value = 0.0;
        double pixel = 0.0;
        QString label;
    };

    void setLabel(QString label) { m_label = std::move(label); }
    QString label() const { return m_label; }

    void setRange(Range range);
    const Range& range() const { return m_range; }
    void extendRange(const Range& range);

    void setPosition(Position position) { m_position = position; }
    Position position() const { return m_position; }

    void setLogScale(bool enabled) { m_logScale = enabled; }
    bool logScale() const { return m_logScale; }

    double project(double pixel, double pixelMin, double pixelMax) const;
    double unproject(double value, double pixelMin, double pixelMax) const;
    std::vector<Tick> ticks(int maxTicks, double pixelMin, double pixelMax) const;

private:
    QString m_label;
    Range m_range;
    Position m_position = Position::Bottom;
    bool m_logScale = false;
};

} // namespace tp::gui::plot
