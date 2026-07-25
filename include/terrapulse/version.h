#pragma once

#define TERRAPULSE_VERSION_MAJOR 0
#define TERRAPULSE_VERSION_MINOR 1
#define TERRAPULSE_VERSION_PATCH 0
#define TERRAPULSE_VERSION_STRING "0.1.0"

namespace tp {

struct Version {
    int major = TERRAPULSE_VERSION_MAJOR;
    int minor = TERRAPULSE_VERSION_MINOR;
    int patch = TERRAPULSE_VERSION_PATCH;
};

inline constexpr Version version() {
    return {};
}

inline constexpr const char* versionString() {
    return TERRAPULSE_VERSION_STRING;
}

} // namespace tp
