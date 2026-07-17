#pragma once
#include "core/ShfRecord.h"
#include <QWidget>

namespace tp {

class EventsPage : public QWidget {
    Q_OBJECT
public:
    explicit EventsPage(QWidget* parent = nullptr);

public slots:
    void onShfReceived(const tp::ShfRecord& shf);
};

} // namespace tp
