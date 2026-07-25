#pragma once

#include "terrapulse/datamodel/event.h"

#include <QString>

namespace tp::gui::datamodel {

struct EventSummary {
    QString publicID;
    QString structureID;
    QString time;
    QString type;
    QString severity;
    QString status;
};

EventSummary summarize(const tp::datamodel::Event& event);

} // namespace tp::gui::datamodel
