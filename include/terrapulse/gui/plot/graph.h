#pragma once

#include "terrapulse/gui/plot/abstractdataset.h"
#include "terrapulse/gui/qt.h"

#include <QColor>
#include <QPen>
#include <QString>

#include <memory>

namespace tp::gui::plot {

class TP_GUI_API Graph {
public:
    explicit Graph(QString name = {});

    QString name() const { return m_name; }
    void setName(QString name) { m_name = std::move(name); }

    void setData(std::shared_ptr<AbstractDataSet> data) { m_data = std::move(data); }
    std::shared_ptr<AbstractDataSet> data() const { return m_data; }

    bool isEmpty() const { return !m_data || m_data->isEmpty(); }
    Range xRange() const { return m_data ? m_data->xRange() : Range(); }
    Range yRange() const { return m_data ? m_data->yRange() : Range(); }
    QPolygonF project(const Axis& xAxis, const Axis& yAxis, double width, double height) const;

    void setColor(QColor color);
    QColor color() const { return m_pen.color(); }
    void setPen(QPen pen) { m_pen = std::move(pen); }
    const QPen& pen() const { return m_pen; }

    bool visible = true;
    bool antiAliasing = true;

private:
    QString m_name;
    std::shared_ptr<AbstractDataSet> m_data;
    QPen m_pen;
};

} // namespace tp::gui::plot
