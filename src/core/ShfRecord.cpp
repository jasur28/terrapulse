#include "core/ShfRecord.h"
#include "core/Crc32.h"

namespace tp {

void ShfRecord::updateCrc() {
    uint32_t c = 0;
    auto h = [&](const void* d, size_t n) { c = tp::crc32(d, n, c ^ 0xFFFFFFFFu) ^ 0xFFFFFFFFu; };
    h(&shfId,            sizeof(shfId));
    h(&sourceSaf,        sizeof(sourceSaf));
    h(&objectId,         sizeof(objectId));
    h(&sensorId,         sizeof(sensorId));
    h(&component,        sizeof(component));
    h(&anomalyType,      sizeof(anomalyType));
    h(&anomalyStartTime, sizeof(anomalyStartTime));
    h(&anomalyEndTime,   sizeof(anomalyEndTime));
    h(&anomalyStatus,    sizeof(anomalyStatus));
    h(&anomalyDuration,  sizeof(anomalyDuration));
    h(&maxValue,         sizeof(maxValue));
    h(&averageValue,     sizeof(averageValue));
    h(&growthRate,       sizeof(growthRate));
    h(&severityLevel,    sizeof(severityLevel));
    h(&confidenceLevel,  sizeof(confidenceLevel));
    h(&trend,            sizeof(trend));
    h(&sampleCount,      sizeof(sampleCount));
    for (const auto& ref : timeline) {
        h(&ref.safId,     sizeof(ref.safId));
        h(&ref.timestamp, sizeof(ref.timestamp));
    }
    if (!notes.empty())
        h(notes.data(), notes.size());
    crc = c;
}

bool ShfRecord::verifyCrc() const {
    ShfRecord tmp = *this;
    tmp.updateCrc();
    return tmp.crc == crc;
}

} // namespace tp
