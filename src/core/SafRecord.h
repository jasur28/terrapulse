#pragma once
#include "core/Types.h"
#include <cstdint>

namespace tp {

// Structural Analysis Format — computed metrics for one SDF window.
struct SafRecord {
    uint8_t     formatVersion       = 1;
    uint64_t    safId               = 0;
    uint64_t    sourceSdf           = 0;
    uint32_t    stationId           = 0;
    uint32_t    objectId            = 0;
    uint32_t    sensorId            = 0;
    Axis        component           = Axis::X;
    int64_t     analysisStartTime   = 0;    // Unix ms
    int64_t     analysisEndTime     = 0;    // Unix ms
    uint32_t    sampleCount         = 0;

    // Signal statistics
    float       rms                 = 0.0f;
    float       variance            = 0.0f;
    float       standardDeviation   = 0.0f;
    float       energy              = 0.0f;
    float       maxAmplitude        = 0.0f;

    // Frequency domain
    float       dominantFrequency   = 0.0f; // Hz
    float       frequencyShift      = 0.0f; // Δ from baseline Hz

    // Health
    float       healthIndex         = 1.0f; // 0.0 critical – 1.0 perfect
    bool        anomalyDetected     = false;
    AnomalyType anomalyType         = AnomalyType::None;
    WarningLevel warningLevel       = WarningLevel::Normal;
    float       confidenceLevel     = 0.0f;

    uint32_t    processingTimeMs    = 0;
    uint32_t    crc                 = 0;

    void updateCrc();
    bool verifyCrc() const;
};

} // namespace tp
