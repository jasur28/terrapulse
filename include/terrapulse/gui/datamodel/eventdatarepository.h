#pragma once

#include "terrapulse/datamodel/event.h"
#include "terrapulse/gui/qt.h"

#include <QString>

#include <memory>
#include <vector>

namespace tp::gui::datamodel {

struct TP_GUI_API EventData {
    std::shared_ptr<tp::datamodel::Event> event;
    bool active = false;
    bool selected = false;
    bool visible = true;
    tp::core::Time containerCreationTime = tp::core::Time::now();

    QString id() const { return event ? event->publicID() : QString(); }
};

class TP_GUI_API EventDataRepository {
public:
    using Container = std::vector<EventData>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    EventData* addEvent(std::shared_ptr<tp::datamodel::Event> event, bool visible = true);
    bool removeEvent(const QString& id);
    EventData* findEvent(const QString& id);
    const EventData* findEvent(const QString& id) const;
    EventData* latestEvent();

    void setEventLifeSpan(tp::core::TimeSpan span) { m_eventLifeSpan = span; }
    EventData* findNextExpiredEvent(tp::core::Time now = tp::core::Time::now());
    void clear();

    iterator begin() { return m_events.begin(); }
    iterator end() { return m_events.end(); }
    const_iterator begin() const { return m_events.begin(); }
    const_iterator end() const { return m_events.end(); }
    int eventCount() const { return static_cast<int>(m_events.size()); }
    std::vector<std::shared_ptr<tp::datamodel::Event>> events() const;

private:
    Container m_events;
    tp::core::TimeSpan m_eventLifeSpan = tp::core::TimeSpan::seconds(24.0 * 60.0 * 60.0);
};

} // namespace tp::gui::datamodel
