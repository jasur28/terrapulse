#pragma once

#include "terrapulse/messaging/message.h"
#include "bus/Journal.h"

namespace tp::messaging {

// Operator journal command (confirm / reject / reclassify / comment) as a
// ready-to-publish message. The audit trail: who did what, when.
inline Message journal(qulonglong eventId, const QString& action,
                       const QString& op, const QString& note = QString()) {
    return tp::journalMessage(eventId, action, op, note);
}

} // namespace tp::messaging
