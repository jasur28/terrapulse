#pragma once

#include "terrapulse/gui/plot/axis.h"
#include "terrapulse/gui/plot/graph.h"
#include "terrapulse/gui/plot/legend.h"
#include "terrapulse/gui/qt.h"

#include <QPolygonF>
#include <QRectF>

#include <memory>
#include <vector>

namespace tp::gui::plot {

class TP_GUI_API Plot {
public:
    Graph* addGraph(QString name = {});
    void clearGraphs();
    void updateRanges();
    std::vector<QPolygonF> project(double width, double height) const;

    Axis xAxis;
    Axis yAxis;
    Axis xAxis2;
    Axis yAxis2;
    Legend legend;

    const std::vector<std::unique_ptr<Graph>>& graphs() const { return m_graphs; }

private:
    std::vector<std::unique_ptr<Graph>> m_graphs;
};

} // namespace tp::gui::plot
