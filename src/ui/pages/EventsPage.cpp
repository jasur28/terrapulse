#include "ui/pages/EventsPage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>

namespace tp {

static const char* severityStr(SeverityLevel s) {
    switch (s) {
    case SeverityLevel::Critical: return "CRITICAL";
    case SeverityLevel::High:     return "HIGH";
    case SeverityLevel::Medium:   return "MEDIUM";
    default:                      return "LOW";
    }
}

static const char* anomalyStr(AnomalyType a) {
    switch (a) {
    case AnomalyType::Vibration:  return "Vibration";
    case AnomalyType::Resonance:  return "Resonance";
    case AnomalyType::Crack:      return "Crack";
    case AnomalyType::Settlement: return "Settlement";
    case AnomalyType::Overload:   return "Overload";
    default:                      return "None";
    }
}

EventsPage::EventsPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* title = new QLabel("Event Log", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* table = new QTableWidget(this);
    table->setObjectName("eventsTable");
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({
        "Start Time", "Object", "Sensor", "Type",
        "Severity", "Duration", "Status"
    });
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table, 1);
}

void EventsPage::onShfReceived(const tp::ShfRecord& shf) {
    auto* table = findChild<QTableWidget*>("eventsTable");
    if (!table) return;

    int row = table->rowCount();
    table->insertRow(row);

    auto set = [&](int col, const QString& text, const QColor& bg = QColor()) {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        if (bg.isValid()) item->setBackground(bg);
        table->setItem(row, col, item);
    };

    QString ts = QDateTime::fromMSecsSinceEpoch(shf.anomalyStartTime).toString("yyyy-MM-dd hh:mm:ss");
    QString dur = (shf.anomalyDuration > 0)
                  ? QString("%1 s").arg(shf.anomalyDuration)
                  : "ongoing";

    QColor sevColor;
    switch (shf.severityLevel) {
    case SeverityLevel::Critical: sevColor = QColor("#FF1744"); break;
    case SeverityLevel::High:     sevColor = QColor("#FF6D00"); break;
    case SeverityLevel::Medium:   sevColor = QColor("#FFD600"); break;
    default:                      sevColor = QColor("#00C853"); break;
    }

    set(0, ts);
    set(1, QString::number(shf.objectId));
    set(2, QString::number(shf.sensorId));
    set(3, anomalyStr(shf.anomalyType));
    set(4, severityStr(shf.severityLevel), sevColor);
    set(5, dur);
    set(6, shf.anomalyStatus == AnomalyStatus::Active ? "ACTIVE" : "RESOLVED");

    table->scrollToBottom();
}

} // namespace tp
