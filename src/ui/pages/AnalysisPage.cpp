#include "ui/pages/AnalysisPage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>

namespace tp {

AnalysisPage::AnalysisPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* title = new QLabel("Analysis History", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* table = new QTableWidget(this);
    table->setObjectName("analysisTable");
    table->setColumnCount(9);
    table->setHorizontalHeaderLabels({
        "Time", "Object", "Sensor", "Axis",
        "RMS", "Max Amp", "Dom. Freq", "Health", "Status"
    });
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table, 1);
}

void AnalysisPage::onSafReceived(const tp::SafRecord& saf) {
    auto* table = findChild<QTableWidget*>("analysisTable");
    if (!table) return;

    int row = table->rowCount();
    table->insertRow(row);

    auto set = [&](int col, const QString& text, const QColor& bg = QColor()) {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        if (bg.isValid()) item->setBackground(bg);
        table->setItem(row, col, item);
    };

    QString ts = QDateTime::fromMSecsSinceEpoch(saf.analysisStartTime).toString("hh:mm:ss");
    static const char* axisName[] = {"X", "Y", "Z"};

    QColor statusColor;
    QString statusText;
    switch (saf.warningLevel) {
    case WarningLevel::Critical: statusColor = QColor("#FF1744"); statusText = "CRITICAL"; break;
    case WarningLevel::Warning:  statusColor = QColor("#FFD600"); statusText = "WARNING";  break;
    default:                     statusColor = QColor("#00C853"); statusText = "NORMAL";   break;
    }

    set(0, ts);
    set(1, QString::number(saf.objectId));
    set(2, QString::number(saf.sensorId));
    set(3, axisName[static_cast<int>(saf.component)]);
    set(4, QString::number(static_cast<double>(saf.rms), 'f', 2));
    set(5, QString::number(static_cast<double>(saf.maxAmplitude), 'f', 2));
    set(6, QString::number(static_cast<double>(saf.dominantFrequency), 'f', 1) + " Hz");
    set(7, QString::number(static_cast<double>(saf.healthIndex), 'f', 3));
    set(8, statusText, statusColor);

    table->scrollToBottom();
}

} // namespace tp
