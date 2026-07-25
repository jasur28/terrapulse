#pragma once

#include "terrapulse/gui/datamodel/stationsymbol.h"

#include <vector>

namespace tp::gui::datamodel {

class EventLayer {
public:
    void setSymbols(std::vector<StationSymbol> symbols) { m_symbols = std::move(symbols); }
    const std::vector<StationSymbol>& symbols() const { return m_symbols; }
    void clear() { m_symbols.clear(); }

private:
    std::vector<StationSymbol> m_symbols;
};

} // namespace tp::gui::datamodel
