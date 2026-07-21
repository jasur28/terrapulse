#pragma once
#include <string>

// The tpmaster broker addresses. A module needs to know only the master's HOST
// (like SeisComp's connection.server) and which QUEUE it talks to; the ports are
// canonical.
//   IN  (XSUB) — modules PUBLISH here    (raw / saf / shf / soh ...)
//   OUT (XPUB) — modules SUBSCRIBE here  (whatever groups they filter)
//
// Two queues, like scmaster:
//   production — the live pipeline (persisted to the main DB)
//   playback   — replay of archived/recorded data (kept OFF the production DB)
namespace tp::master {

enum class Queue { Production, Playback };

inline int inPort (Queue q) { return q == Queue::Playback ? 5571 : 5561; }
inline int outPort(Queue q) { return q == Queue::Playback ? 5572 : 5562; }

// Control (REQ/REP): a client asks the master for a snapshot of the current
// data model (inventory + latest state) so its pages fill in immediately.
inline constexpr int kCtrlPort = 5563;
inline std::string ctrl(const std::string& host) {
    return "tcp://" + host + ":" + std::to_string(kCtrlPort);
}

inline std::string in (const std::string& host, Queue q = Queue::Production) {
    return "tcp://" + host + ":" + std::to_string(inPort(q));
}
inline std::string out(const std::string& host, Queue q = Queue::Production) {
    return "tcp://" + host + ":" + std::to_string(outPort(q));
}

inline Queue queueFromName(const std::string& s) {
    return s == "playback" ? Queue::Playback : Queue::Production;
}
inline const char* queueName(Queue q) {
    return q == Queue::Playback ? "playback" : "production";
}

} // namespace tp::master
