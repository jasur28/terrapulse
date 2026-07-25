#pragma once

#include "terrapulse/datamodel/inventory.h"
#include "terrapulse/gui/qt.h"

#include <QAbstractListModel>

#include <memory>
#include <vector>

namespace tp::gui::datamodel {

class TP_GUI_API InventoryListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        PublicIDRole = Qt::UserRole + 1,
        CodeRole,
        NameRole,
        TypeRole,
        LatitudeRole,
        LongitudeRole,
        SensorCountRole
    };

    explicit InventoryListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setInventory(const tp::datamodel::Inventory& inventory);
    void clear();

private:
    std::vector<std::shared_ptr<tp::datamodel::Structure>> m_structures;
};

} // namespace tp::gui::datamodel
