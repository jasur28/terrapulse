#include "ui/MainWindow.h"
#include "ui/pages/DashboardPage.h"
#include "ui/pages/MonitoringPage.h"
#include "ui/pages/ObjectsPage.h"
#include "ui/pages/SensorsPage.h"
#include "ui/pages/AnalysisPage.h"
#include "ui/pages/EventsPage.h"
#include "ui/pages/SettingsPage.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>

namespace tp {

static const QStringList NAV_LABELS = {
    "Dashboard", "Monitoring", "Objects",
    "Sensors",   "Analysis",  "Events", "Settings"
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("TerraPulse — Structural Health Monitor");
    resize(1280, 760);
    applyTheme();
    buildUi();
}

void MainWindow::applyTheme() {
    qApp->setStyle("Fusion");
    QFile f(":/styles/dark_theme.qss");
    if (f.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    buildNavigation();
    root->addWidget(m_navPanel);

    m_dashboard  = new DashboardPage(this);
    m_monitoring = new MonitoringPage(this);
    m_objects    = new ObjectsPage(this);
    m_sensors    = new SensorsPage(this);
    m_analysis   = new AnalysisPage(this);
    m_events     = new EventsPage(this);
    m_settings   = new SettingsPage(this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_dashboard);
    m_stack->addWidget(m_monitoring);
    m_stack->addWidget(m_objects);
    m_stack->addWidget(m_sensors);
    m_stack->addWidget(m_analysis);
    m_stack->addWidget(m_events);
    m_stack->addWidget(m_settings);

    root->addWidget(m_stack, 1);

    // Connect pipeline signals
    connect(this, &MainWindow::safReceived, m_dashboard,  &DashboardPage::onSafReceived);
    connect(this, &MainWindow::safReceived, m_monitoring, &MonitoringPage::onSafReceived);
    connect(this, &MainWindow::safReceived, m_analysis,   &AnalysisPage::onSafReceived);
    connect(this, &MainWindow::shfReceived, m_events,     &EventsPage::onShfReceived);

    navigateTo(0);
}

void MainWindow::buildNavigation() {
    m_navPanel = new QWidget(this);
    m_navPanel->setObjectName("navPanel");
    m_navPanel->setFixedWidth(180);

    auto* layout = new QVBoxLayout(m_navPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* logo = new QLabel("TerraPulse", m_navPanel);
    logo->setObjectName("logoLabel");
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedHeight(56);
    layout->addWidget(logo);

    for (int i = 0; i < NAV_LABELS.size(); ++i) {
        auto* btn = new QPushButton(NAV_LABELS[i], m_navPanel);
        btn->setObjectName("navButton");
        btn->setCheckable(true);
        btn->setFixedHeight(44);
        connect(btn, &QPushButton::clicked, this, [this, i]{ navigateTo(i); });
        m_navBtns.append(btn);
        layout->addWidget(btn);
    }
    layout->addStretch();
}

void MainWindow::navigateTo(int index) {
    for (int i = 0; i < m_navBtns.size(); ++i)
        m_navBtns[i]->setChecked(i == index);
    m_stack->setCurrentIndex(index);
}

} // namespace tp
