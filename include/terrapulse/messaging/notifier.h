#pragma once

#include "terrapulse/messaging/message.h"
#include "bus/Notifier.h"

namespace tp::messaging {

using Op = tp::Op;

// Data-model change (add / update / remove of an inventory object, etc.) as a
// ready-to-publish message on the relevant group.
inline Message notifier(Op op, const QString& kind, const QString& key,
                        const QVariantMap& fields) {
    return tp::notifier(op, kind, key, fields);
}

inline const char* opName(Op op) { return tp::opName(op); }

} // namespace tp::messaging
