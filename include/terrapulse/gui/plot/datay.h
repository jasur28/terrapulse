#pragma once

#include "terrapulse/gui/plot/abstractdataset.h"

#include <QVector>

namespace tp::gui::plot {

class TP_GUI_API DataY : public AbstractDataSet {
public:
    int count() const override { return y.size(); }
    Range xRange() const override { return x; }
    Range yRange() const override;
    void clear() override { y.clear(); }
    QPolygonF project(const Axis& xAxis, const Axis& yAxis,
                      double width, double height) const override;

    Range x;
    QVector<double> y;
};

} // namespace tp::gui::plot
