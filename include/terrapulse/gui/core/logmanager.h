#pragma once

#include "terrapulse/gui/qt.h"

#include <QString>

#include <deque>

namespace tp::gui {

struct LogEntry {
    QString level;
    QString module;
    QString message;
};

class TP_GUI_API LogManager {
public:
    void append(QString level, QString module, QString message);
    void clear() { m_entries.clear(); }
    const std::deque<LogEntry>& entries() const { return m_entries; }
    void setCapacity(std::size_t capacity) { m_capacity = capacity; trim(); }

private:
    void trim();

    std::deque<LogEntry> m_entries;
    std::size_t m_capacity = 1000;
};

} // namespace tp::gui
