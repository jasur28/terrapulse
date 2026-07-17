#pragma once
#include "core/Types.h"
#include <QLabel>

namespace tp {

class StatusBadge : public QLabel {
    Q_OBJECT
public:
    explicit StatusBadge(QWidget* parent = nullptr);
    void setStatus(WarningLevel level);
    void setOffline();
};

} // namespace tp
