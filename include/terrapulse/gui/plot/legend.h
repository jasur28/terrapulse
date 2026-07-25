#pragma once

#include "terrapulse/gui/plot/abstractlegend.h"

namespace tp::gui::plot {

class TP_GUI_API Legend : public AbstractLegend {
public:
    void setItems(std::vector<LegendItem> items) { m_items = std::move(items); }
    std::vector<LegendItem> items() const override { return m_items; }

private:
    std::vector<LegendItem> m_items;
};

} // namespace tp::gui::plot
