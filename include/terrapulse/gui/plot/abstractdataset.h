#pragma once

#include "terrapulse/gui/plot/range.h"
#include "terrapulse/gui/qt.h"

#include <QPointF>
#include <QPolygonF>

namespace tp::gui::plot {

class Axis;

class TP_GUI_API AbstractDataSet {
public:
    virtual ~AbstractDataSet() = default;

    bool isEmpty() const { return count() == 0; }
    virtual int count() const = 0;
    virtual Range xRange() const = 0;
    virtual Range yRange() const = 0;
    virtual void clear() = 0;
    virtual QPolygonF project(const Axis& xAxis, const Axis& yAxis,
                              double width, double height) const = 0;
};

} // namespace tp::gui::plot
