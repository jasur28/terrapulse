#pragma once
#include "core/SafRecord.h"
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QMap>

namespace tp {

class MonitoringPage : public QWidget {
    Q_OBJECT
public:
    explicit MonitoringPage(QWidget* parent = nullptr);

public slots:
    void onSafReceived(const tp::SafRecord& saf);

private:
    void buildUi();
    QChartView* m_rmsChart     = nullptr;
    QChartView* m_healthChart  = nullptr;

    QLineSeries* m_rmsSeriesX  = nullptr;
    QLineSeries* m_rmsSeriesY  = nullptr;
    QLineSeries* m_rmsSeriesZ  = nullptr;
    QLineSeries* m_healthSeries = nullptr;

    int64_t m_lastTime = 0;
    static constexpr int MAX_POINTS = 200;
};

} // namespace tp
