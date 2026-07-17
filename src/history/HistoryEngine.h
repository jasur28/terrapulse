#pragma once
#include "core/SafRecord.h"
#include "core/ShfRecord.h"
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace tp {

class HistoryEngine {
public:
    // Feed an analysis result. Returns a new/updated ShfRecord if an anomaly
    // event was opened or modified, otherwise returns an empty optional.
    std::optional<ShfRecord> processSaf(const SafRecord& saf);

    // Returns all currently active anomaly events.
    const std::vector<ShfRecord>& activeEvents() const { return m_activeEvents; }

    // Returns all completed anomaly events.
    const std::vector<ShfRecord>& resolvedEvents() const { return m_resolvedEvents; }

private:
    std::vector<ShfRecord> m_activeEvents;
    std::vector<ShfRecord> m_resolvedEvents;
    uint64_t               m_nextShfId = 1;

    // Key: sensorId * 10 + axis
    std::unordered_map<uint64_t, size_t> m_activeIndex;

    uint64_t makeKey(uint32_t sensorId, Axis axis) const;
    ShfRecord openEvent(const SafRecord& saf);
    void      updateEvent(ShfRecord& event, const SafRecord& saf);
    Trend     computeTrend(const ShfRecord& event) const;
    SeverityLevel computeSeverity(WarningLevel wl, float health) const;
};

} // namespace tp
