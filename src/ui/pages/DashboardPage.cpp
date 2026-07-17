#include "ui/pages/DashboardPage.h"
#include "ui/widgets/HealthRingWidget.h"
#include "ui/widgets/StatusBadge.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QMap>

namespace tp {

DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void DashboardPage::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* title = new QLabel("Dashboard", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);

    m_cardsContainer = new QWidget(m_scroll);
    m_cardsContainer->setObjectName("cardsContainer");
    auto* grid = new QHBoxLayout(m_cardsContainer);
    grid->setContentsMargins(0, 10, 0, 0);
    grid->setSpacing(16);
    grid->addStretch();

    m_scroll->setWidget(m_cardsContainer);
    layout->addWidget(m_scroll, 1);
}

void DashboardPage::onSafReceived(const tp::SafRecord& saf) {
    Q_UNUSED(saf)
    // TODO: update object card for saf.objectId
}

} // namespace tp
