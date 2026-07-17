#include "core/SafRecord.h"
#include "core/Crc32.h"

namespace tp {

void SafRecord::updateCrc() {
    // CRC covers all numeric fields (not crc itself).
    // We hash the struct up to (but not including) the crc field.
    // Simplest portable approach: hash each field individually.
    uint32_t c = 0;
    auto h = [&](const void* d, size_t n) { c = tp::crc32(d, n, c ^ 0xFFFFFFFFu) ^ 0xFFFFFFFFu; };
    h(&safId,             sizeof(safId));
    h(&sourceSdf,         sizeof(sourceSdf));
    h(&stationId,         sizeof(stationId));
    h(&objectId,          sizeof(objectId));
    h(&sensorId,          sizeof(sensorId));
    h(&component,         sizeof(component));
    h(&analysisStartTime, sizeof(analysisStartTime));
    h(&analysisEndTime,   sizeof(analysisEndTime));
    h(&sampleCount,       sizeof(sampleCount));
    h(&rms,               sizeof(rms));
    h(&variance,          sizeof(variance));
    h(&standardDeviation, sizeof(standardDeviation));
    h(&energy,            sizeof(energy));
    h(&maxAmplitude,      sizeof(maxAmplitude));
    h(&dominantFrequency, sizeof(dominantFrequency));
    h(&frequencyShift,    sizeof(frequencyShift));
    h(&healthIndex,       sizeof(healthIndex));
    h(&anomalyDetected,   sizeof(anomalyDetected));
    h(&anomalyType,       sizeof(anomalyType));
    h(&warningLevel,      sizeof(warningLevel));
    h(&confidenceLevel,   sizeof(confidenceLevel));
    crc = c;
}

bool SafRecord::verifyCrc() const {
    SafRecord tmp = *this;
    tmp.updateCrc();
    return tmp.crc == crc;
}

} // namespace tp
