#pragma once

#include "terrapulse/gui/plot/abstractdataset.h"

#include <QVector>

namespace tp::gui::plot {

class TP_GUI_API DataXY : public AbstractDataSet {
public:
    using DataPoint = QPointF;
    using DataPoints = QVector<DataPoint>;

    int count() const override { return data.size(); }
    Range xRange() const override;
    Range yRange() const override;
    void clear() override { data.clear(); }
    QPolygonF project(const Axis& xAxis, const Axis& yAxis,
                      double width, double height) const override;

    DataPoints data;
};

} // namespace tp::gui::plot
