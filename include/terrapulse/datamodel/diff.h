#pragma once

#include "terrapulse/datamodel/notifier.h"

#include <vector>

namespace tp::datamodel {

class Diff {
public:
    void add(const Notifier& notifier) { m_notifiers.push_back(notifier); }
    bool empty() const { return m_notifiers.empty(); }
    std::size_t size() const { return m_notifiers.size(); }
    void clear() { m_notifiers.clear(); }
    const std::vector<Notifier>& notifiers() const { return m_notifiers; }

private:
    std::vector<Notifier> m_notifiers;
};

} // namespace tp::datamodel
