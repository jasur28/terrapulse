#include "ui/pages/SettingsPage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QGroupBox>

namespace tp {

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* title = new QLabel("Settings", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* threshGroup = new QGroupBox("Anomaly Thresholds", this);
    auto* form = new QFormLayout(threshGroup);

    auto addRow = [&](const QString& label, double val, double min, double max) {
        auto* sb = new QDoubleSpinBox();
        sb->setRange(min, max); sb->setValue(val);
        form->addRow(label, sb);
    };

    addRow("RMS Warning factor", 2.0, 1.0, 10.0);
    addRow("RMS Critical factor", 4.0, 1.0, 20.0);
    addRow("Amplitude Warning (mg)", 50.0, 1.0, 1000.0);
    addRow("Amplitude Critical (mg)", 100.0, 1.0, 1000.0);
    addRow("Freq Shift Warning (Hz)", 5.0, 0.5, 50.0);
    addRow("Freq Shift Critical (Hz)", 15.0, 1.0, 100.0);
    addRow("Health Index Warning", 0.6, 0.1, 1.0);
    addRow("Health Index Critical", 0.3, 0.05, 1.0);

    layout->addWidget(threshGroup);

    auto* saveBtn = new QPushButton("Save Settings", this);
    saveBtn->setObjectName("primaryButton");
    layout->addWidget(saveBtn);

    layout->addStretch();
}

} // namespace tp
