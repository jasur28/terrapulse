#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QTimer>
#include "core/SafRecord.h"
#include "core/ShfRecord.h"

namespace tp {

class DashboardPage;
class MonitoringPage;
class ObjectsPage;
class SensorsPage;
class AnalysisPage;
class EventsPage;
class SettingsPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

signals:
    void safReceived(const tp::SafRecord& saf);
    void shfReceived(const tp::ShfRecord& shf);

private:
    void buildUi();
    void buildNavigation();
    void applyTheme();
    void navigateTo(int index);

    QWidget*       m_navPanel     = nullptr;
    QStackedWidget* m_stack       = nullptr;
    QList<QPushButton*> m_navBtns;

    DashboardPage*  m_dashboard  = nullptr;
    MonitoringPage* m_monitoring = nullptr;
    ObjectsPage*    m_objects    = nullptr;
    SensorsPage*    m_sensors    = nullptr;
    AnalysisPage*   m_analysis   = nullptr;
    EventsPage*     m_events     = nullptr;
    SettingsPage*   m_settings   = nullptr;
};

} // namespace tp
