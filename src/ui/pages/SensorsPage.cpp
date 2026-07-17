#include "ui/pages/SensorsPage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>

namespace tp {

SensorsPage::SensorsPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* title = new QLabel("Sensors", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* table = new QTableWidget(this);
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({"ID", "Object", "Axis", "Status", "Battery %", "Temp °C", "Last Seen"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table, 1);
}

} // namespace tp
