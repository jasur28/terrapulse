#pragma once
#include "core/Types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace tp {

struct SafRef {
    uint64_t safId     = 0;
    int64_t  timestamp = 0; // Unix ms
};

// Structural History Format — anomaly event record with timeline.
struct ShfRecord {
    uint8_t       formatVersion     = 1;
    uint64_t      shfId             = 0;
    uint64_t      sourceSaf         = 0;
    uint32_t      objectId          = 0;
    uint32_t      sensorId          = 0;
    Axis          component         = Axis::X;
    AnomalyType   anomalyType       = AnomalyType::None;
    int64_t       anomalyStartTime  = 0;    // Unix ms
    int64_t       anomalyEndTime    = 0;    // Unix ms, 0 = ongoing
    AnomalyStatus anomalyStatus     = AnomalyStatus::Active;
    uint32_t      anomalyDuration   = 0;    // seconds
    float         maxValue          = 0.0f;
    float         averageValue      = 0.0f;
    float         growthRate        = 0.0f; // per hour
    SeverityLevel severityLevel     = SeverityLevel::Low;
    float         confidenceLevel   = 0.0f;
    Trend         trend             = Trend::Unknown;
    uint32_t      sampleCount       = 0;
    std::vector<SafRef> timeline;
    std::string   notes;
    uint32_t      crc               = 0;

    void updateCrc();
    bool verifyCrc() const;
};

} // namespace tp
