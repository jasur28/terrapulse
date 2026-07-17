#include "ui/widgets/StatusBadge.h"
#include <QHBoxLayout>

namespace tp {

StatusBadge::StatusBadge(QWidget* parent) : QLabel(parent) {
    setAlignment(Qt::AlignCenter);
    setFixedSize(90, 24);
    setStatus(WarningLevel::Normal);
}

void StatusBadge::setStatus(WarningLevel level) {
    switch (level) {
    case WarningLevel::Critical:
        setText("CRITICAL");
        setStyleSheet("background:#FF1744; color:#fff; border-radius:4px; font-weight:bold; font-size:11px;");
        break;
    case WarningLevel::Warning:
        setText("WARNING");
        setStyleSheet("background:#FFD600; color:#000; border-radius:4px; font-weight:bold; font-size:11px;");
        break;
    default:
        setText("NORMAL");
        setStyleSheet("background:#00C853; color:#fff; border-radius:4px; font-weight:bold; font-size:11px;");
        break;
    }
}

void StatusBadge::setOffline() {
    setText("OFFLINE");
    setStyleSheet("background:#616161; color:#E0E0E0; border-radius:4px; font-weight:bold; font-size:11px;");
}

} // namespace tp
