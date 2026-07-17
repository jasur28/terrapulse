#pragma once
#include <cstdint>

namespace tp {

enum class Axis : uint8_t {
    X = 0,
    Y = 1,
    Z = 2
};

enum class MeasurementUnit : uint8_t {
    Millig  = 0,  // mg
    MetersPerSecondSquared = 1   // m/s²
};

enum class SensorStatus : uint8_t {
    Ok      = 0,
    Warning = 1,
    Error   = 2,
    Offline = 3
};

enum class AnomalyType : uint8_t {
    None       = 0,
    Vibration  = 1,
    Resonance  = 2,
    Crack      = 3,
    Settlement = 4,
    Overload   = 5
};

enum class WarningLevel : uint8_t {
    Normal   = 0,
    Warning  = 1,
    Critical = 2
};

enum class AnomalyStatus : uint8_t {
    Active     = 0,
    Resolved   = 1,
    Monitoring = 2
};

enum class SeverityLevel : uint8_t {
    Low      = 0,
    Medium   = 1,
    High     = 2,
    Critical = 3
};

enum class Trend : uint8_t {
    Stable    = 0,
    Improving = 1,
    Worsening = 2,
    Unknown   = 3
};

} // namespace tp
