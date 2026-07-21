#pragma once
#include <array>
#include <string>
#include <string_view>

// Canonical TerraPulse message groups (SeisComp scmaster-style).
// Every module talks to ONE central broker (tpmaster); the topic prefix is the
// group name on the wire and decides routing. Clients subscribe to the groups
// they care about — nothing else changes when the topology grows.
namespace tp::groups {

struct Group {
    std::string_view name;    // logical group name
    std::string_view prefix;  // topic prefix on the bus
    std::string_view desc;    // human description
};

inline constexpr std::array<Group, 7> kAll = {{
    { "WAVEFORM",  "raw.",  "Raw acceleration samples          (tpacq -> ...)" },
    { "FEATURE",   "saf.",  "Analysis feature sets / SAF       (tpproc -> ...)" },
    { "ANOMALY",   "shf.",  "Anomaly & history events / SHF    (tpproc -> ...)" },
    { "STATUS",    "soh.",  "State-of-health / heartbeats      (all modules)" },
    { "CONFIG",    "cfg.",  "Configuration updates             (control -> ...)" },
    { "INVENTORY", "inv.",  "Inventory notifiers               (structures/sensors/channels)" },
    { "JOURNAL",   "jrnl.", "Operator actions / audit          (confirm/reject/comment)" },
}};

// Index of the group owning `topic` (longest matching prefix), or -1 = OTHER.
inline int indexForTopic(std::string_view topic) {
    for (std::size_t i = 0; i < kAll.size(); ++i) {
        const auto& p = kAll[i].prefix;
        if (topic.size() >= p.size() && topic.compare(0, p.size(), p) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

inline std::string_view groupForTopic(std::string_view topic) {
    const int i = indexForTopic(topic);
    return i < 0 ? std::string_view{"OTHER"} : kAll[i].name;
}

} // namespace tp::groups
