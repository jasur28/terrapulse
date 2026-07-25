#pragma once

#include "bus/Master.h"

// Broker endpoint addressing (which host/port/queue a module connects to). This
// is configuration, not transport — most modules get it via ApplicationSettings,
// but a relay or a tool that talks to a SECOND master needs it directly.
namespace tp::messaging {

using Queue = tp::master::Queue;

inline std::string in(const std::string& host, Queue q = Queue::Production) {
    return tp::master::in(host, q);
}
inline std::string out(const std::string& host, Queue q = Queue::Production) {
    return tp::master::out(host, q);
}
inline Queue queueFromName(const std::string& name) { return tp::master::queueFromName(name); }
inline const char* queueName(Queue q) { return tp::master::queueName(q); }

} // namespace tp::messaging
