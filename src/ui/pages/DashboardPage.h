#pragma once
#include "core/SafRecord.h"
#include <QWidget>
#include <QScrollArea>

namespace tp {

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);

public slots:
    void onSafReceived(const tp::SafRecord& saf);

private:
    void buildUi();
    QWidget*     m_cardsContainer = nullptr;
    QScrollArea* m_scroll         = nullptr;
};

} // namespace tp
