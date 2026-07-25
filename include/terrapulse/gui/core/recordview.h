#pragma once

#include "terrapulse/gui/core/recordviewitem.h"
#include "terrapulse/gui/qt.h"

#include <memory>
#include <vector>

namespace tp::gui {

class TP_GUI_API RecordViewModel {
public:
    enum class SelectionMode {
        NoSelection,
        SingleSelection,
        ExtendedSelection
    };

    RecordViewItem* addItem(tp::datamodel::WaveformStreamID streamID, QString label);
    bool addRecord(std::shared_ptr<tp::core::Record> record);
    void clear();
    void clearRecords();
    int rowCount() const { return static_cast<int>(m_items.size()); }
    RecordViewItem* itemAt(int row) const;
    RecordViewItem* item(const QString& streamID) const;
    const std::vector<std::unique_ptr<RecordViewItem>>& items() const { return m_items; }

private:
    std::vector<std::unique_ptr<RecordViewItem>> m_items;
};

} // namespace tp::gui
