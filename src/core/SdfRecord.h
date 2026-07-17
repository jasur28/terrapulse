#pragma once
#include "core/Types.h"
#include <vector>
#include <cstdint>

namespace tp {

// Structural Data Format — raw sensor telemetry for one measurement window.
struct SdfRecord {
    uint8_t         formatVersion   = 1;
    uint32_t        stationId       = 0;
    uint32_t        objectId        = 0;
    uint32_t        sensorId        = 0;
    Axis            component       = Axis::X;
    uint32_t        samplingRate    = 100;      // Hz
    MeasurementUnit unit            = MeasurementUnit::Millig;
    int64_t         startTime       = 0;        // Unix ms
    int64_t         endTime         = 0;        // Unix ms
    uint32_t        sampleCount     = 0;
    SensorStatus    sensorStatus    = SensorStatus::Ok;
    float           temperature     = 0.0f;     // °C
    uint8_t         batteryLevel    = 100;      // 0–100 %
    uint32_t        crc             = 0;
    std::vector<float> waveformData;

    // Recomputes crc from waveformData and stores it.
    void updateCrc();

    // Returns true if stored crc matches waveformData.
    bool verifyCrc() const;

    // Window duration in milliseconds.
    int64_t durationMs() const { return endTime - startTime; }
};

} // namespace tp
