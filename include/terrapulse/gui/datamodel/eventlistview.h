#pragma once

#include "terrapulse/datamodel/event.h"
#include "terrapulse/geo/boundingbox.h"
#include "terrapulse/gui/datamodel/eventsummary.h"
#include "terrapulse/gui/qt.h"

#include <QAbstractListModel>

#include <memory>
#include <optional>
#include <vector>

namespace tp::gui::datamodel {

class TP_GUI_API EventListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        PublicIDRole = Qt::UserRole + 1,
        StructureIDRole,
        TimeRole,
        TypeRole,
        SeverityRole,
        StatusRole
    };

    explicit EventListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEvents(std::vector<std::shared_ptr<tp::datamodel::Event>> events);
    void clear();

private:
    std::vector<std::shared_ptr<tp::datamodel::Event>> m_events;
};

struct EventFilter {
    tp::core::TimeWindow timeWindow;
    std::optional<tp::geo::BoundingBox> region;
    QString eventID;
};

} // namespace tp::gui::datamodel
