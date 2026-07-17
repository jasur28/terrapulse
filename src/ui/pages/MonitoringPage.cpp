#include "ui/pages/MonitoringPage.h"
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QLabel>
#include <QSplitter>

namespace tp {

MonitoringPage::MonitoringPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void MonitoringPage::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* title = new QLabel("Live Monitoring", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // RMS chart
    m_rmsSeriesX = new QLineSeries();  m_rmsSeriesX->setName("X");
    m_rmsSeriesY = new QLineSeries();  m_rmsSeriesY->setName("Y");
    m_rmsSeriesZ = new QLineSeries();  m_rmsSeriesZ->setName("Z");
    m_rmsSeriesX->setColor(QColor("#FF4081"));
    m_rmsSeriesY->setColor(QColor("#00E5FF"));
    m_rmsSeriesZ->setColor(QColor("#00C853"));

    auto* rmsChart = new QChart();
    rmsChart->setTitle("RMS Acceleration (mg)");
    rmsChart->addSeries(m_rmsSeriesX);
    rmsChart->addSeries(m_rmsSeriesY);
    rmsChart->addSeries(m_rmsSeriesZ);
    rmsChart->createDefaultAxes();
    rmsChart->setBackgroundBrush(QBrush(QColor("#16213E")));
    rmsChart->setTitleBrush(QBrush(QColor("#E0E0E0")));
    rmsChart->legend()->setLabelColor(QColor("#E0E0E0"));

    m_rmsChart = new QChartView(rmsChart, this);
    m_rmsChart->setRenderHint(QPainter::Antialiasing);

    // Health index chart
    m_healthSeries = new QLineSeries();
    m_healthSeries->setName("Health Index");
    m_healthSeries->setColor(QColor("#00C853"));

    auto* healthChart = new QChart();
    healthChart->setTitle("Health Index");
    healthChart->addSeries(m_healthSeries);
    healthChart->createDefaultAxes();
    healthChart->setBackgroundBrush(QBrush(QColor("#16213E")));
    healthChart->setTitleBrush(QBrush(QColor("#E0E0E0")));
    healthChart->legend()->setLabelColor(QColor("#E0E0E0"));
    // Fix Y axis to 0–1
    auto* yAxis = qobject_cast<QValueAxis*>(healthChart->axes(Qt::Vertical).first());
    if (yAxis) { yAxis->setRange(0.0, 1.0); }

    m_healthChart = new QChartView(healthChart, this);
    m_healthChart->setRenderHint(QPainter::Antialiasing);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_rmsChart);
    splitter->addWidget(m_healthChart);
    layout->addWidget(splitter, 1);
}

void MonitoringPage::onSafReceived(const tp::SafRecord& saf) {
    double x = static_cast<double>(saf.analysisStartTime) / 1000.0; // seconds

    if (saf.component == Axis::X) m_rmsSeriesX->append(x, saf.rms);
    if (saf.component == Axis::Y) m_rmsSeriesY->append(x, saf.rms);
    if (saf.component == Axis::Z) m_rmsSeriesZ->append(x, saf.rms);

    m_healthSeries->append(x, saf.healthIndex);

    // Trim to MAX_POINTS
    auto trim = [](QLineSeries* s) {
        while (s->count() > MAX_POINTS) s->remove(0);
    };
    trim(m_rmsSeriesX); trim(m_rmsSeriesY); trim(m_rmsSeriesZ);
    trim(m_healthSeries);

    // Scroll X axes to latest data
    auto scrollAxis = [](QLineSeries* s, QChart* chart) {
        if (s->count() < 2) return;
        auto pts = s->points();
        double xMin = pts.first().x();
        double xMax = pts.last().x();
        auto axes = chart->axes(Qt::Horizontal);
        if (!axes.isEmpty())
            qobject_cast<QValueAxis*>(axes.first())->setRange(xMin, xMax);
    };
    scrollAxis(m_rmsSeriesZ, m_rmsChart->chart());
    scrollAxis(m_healthSeries, m_healthChart->chart());
}

} // namespace tp
