#include "analysis/AnalysisEngine.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <chrono>

namespace tp {

AnalysisEngine::AnalysisEngine(const AnalysisThresholds& t) : m_thresholds(t) {}

float AnalysisEngine::computeRms(const std::vector<float>& data) {
    if (data.empty()) return 0.0f;
    double sum = 0.0;
    for (float v : data) sum += static_cast<double>(v) * v;
    return static_cast<float>(std::sqrt(sum / data.size()));
}

float AnalysisEngine::computeVariance(const std::vector<float>& data, float mean) {
    if (data.empty()) return 0.0f;
    double sum = 0.0;
    for (float v : data) { double d = v - mean; sum += d * d; }
    return static_cast<float>(sum / data.size());
}

float AnalysisEngine::computeEnergy(const std::vector<float>& data) {
    if (data.empty()) return 0.0f;
    double sum = 0.0;
    for (float v : data) sum += static_cast<double>(v) * v;
    return static_cast<float>(sum);
}

float AnalysisEngine::computeMaxAmplitude(const std::vector<float>& data) {
    if (data.empty()) return 0.0f;
    float maxAbs = 0.0f;
    for (float v : data) maxAbs = std::max(maxAbs, std::abs(v));
    return maxAbs;
}

float AnalysisEngine::computeDominantFrequency(const std::vector<float>& data, uint32_t samplingRate) {
    // Zero-crossing rate method — reliable, no external dependencies.
    if (data.size() < 4 || samplingRate == 0) return 0.0f;
    int crossings = 0;
    for (size_t i = 1; i < data.size(); ++i) {
        if ((data[i - 1] >= 0.0f) != (data[i] >= 0.0f))
            ++crossings;
    }
    float duration = static_cast<float>(data.size()) / static_cast<float>(samplingRate);
    return static_cast<float>(crossings) / (2.0f * duration);
}

float AnalysisEngine::computeHealthIndex(float rms, float energy, float maxAmp, float freqShift) const {
    const auto& t = m_thresholds;
    auto norm = [](float v, float warnLevel) -> float {
        if (warnLevel <= 0.0f) return 0.0f;
        return std::min(v / warnLevel, 2.0f) * 0.5f; // 0..1
    };
    float score =
        t.wRms    * norm(rms,      t.baselineRms * t.rmsWarningFactor) +
        t.wEnergy * norm(energy,   t.baselineEnergy * t.energyWarningFactor) +
        t.wAmp    * norm(maxAmp,   t.ampWarning) +
        t.wFreq   * norm(freqShift, t.freqShiftWarning);
    return std::max(0.0f, std::min(1.0f, 1.0f - score));
}

void AnalysisEngine::classifyAnomaly(SafRecord& out) const {
    const auto& t = m_thresholds;
    out.anomalyDetected = false;
    out.anomalyType     = AnomalyType::None;
    out.warningLevel    = WarningLevel::Normal;

    // Check overload first (highest priority).
    if (out.maxAmplitude >= t.ampCritical) {
        out.anomalyDetected = true;
        out.anomalyType     = AnomalyType::Overload;
        out.warningLevel    = WarningLevel::Critical;
        out.confidenceLevel = 0.95f;
        return;
    }

    // Crack: high-frequency burst (dominant > 40 Hz) + amplitude spike.
    if (out.dominantFrequency > 40.0f && out.maxAmplitude >= t.ampWarning) {
        out.anomalyDetected = true;
        out.anomalyType     = AnomalyType::Crack;
        out.warningLevel    = (out.maxAmplitude >= t.ampCritical) ? WarningLevel::Critical : WarningLevel::Warning;
        out.confidenceLevel = 0.75f;
        return;
    }

    // Resonance: dominant frequency shift exceeds critical.
    if (std::abs(out.frequencyShift) >= t.freqShiftCritical) {
        out.anomalyDetected = true;
        out.anomalyType     = AnomalyType::Resonance;
        out.warningLevel    = WarningLevel::Critical;
        out.confidenceLevel = 0.80f;
        return;
    }

    // Settlement: significant low-frequency shift over time (warning level).
    if (std::abs(out.frequencyShift) >= t.freqShiftWarning) {
        out.anomalyDetected = true;
        out.anomalyType     = AnomalyType::Settlement;
        out.warningLevel    = WarningLevel::Warning;
        out.confidenceLevel = 0.65f;
        return;
    }

    // Vibration: RMS or energy exceedance.
    bool rmsWarn = out.rms >= t.baselineRms * t.rmsWarningFactor;
    bool rmsCrit = out.rms >= t.baselineRms * t.rmsCriticalFactor;
    bool engWarn = out.energy >= t.baselineEnergy * t.energyWarningFactor;
    bool engCrit = out.energy >= t.baselineEnergy * t.energyCriticalFactor;

    if (rmsCrit || engCrit) {
        out.anomalyDetected = true;
        out.anomalyType     = AnomalyType::Vibration;
        out.warningLevel    = WarningLevel::Critical;
        out.confidenceLevel = 0.90f;
    } else if (rmsWarn || engWarn) {
        out.anomalyDetected = true;
        out.anomalyType     = AnomalyType::Vibration;
        out.warningLevel    = WarningLevel::Warning;
        out.confidenceLevel = 0.70f;
    }
}

SafRecord AnalysisEngine::analyse(const SdfRecord& sdf, uint64_t safId) const {
    auto t0 = std::chrono::steady_clock::now();

    SafRecord out;
    out.safId             = safId;
    out.sourceSdf         = sdf.sensorId; // caller may override with SDF record ID
    out.stationId         = sdf.stationId;
    out.objectId          = sdf.objectId;
    out.sensorId          = sdf.sensorId;
    out.component         = sdf.component;
    out.analysisStartTime = sdf.startTime;
    out.analysisEndTime   = sdf.endTime;
    out.sampleCount       = static_cast<uint32_t>(sdf.waveformData.size());

    const auto& d = sdf.waveformData;
    float mean = d.empty() ? 0.0f : std::accumulate(d.begin(), d.end(), 0.0f) / d.size();

    out.rms               = computeRms(d);
    out.variance          = computeVariance(d, mean);
    out.standardDeviation = std::sqrt(out.variance);
    out.energy            = computeEnergy(d);
    out.maxAmplitude      = computeMaxAmplitude(d);
    out.dominantFrequency = computeDominantFrequency(d, sdf.samplingRate);
    out.frequencyShift    = (m_thresholds.baselineFreq > 0.0f)
                              ? (out.dominantFrequency - m_thresholds.baselineFreq)
                              : 0.0f;
    out.healthIndex       = computeHealthIndex(out.rms, out.energy, out.maxAmplitude, std::abs(out.frequencyShift));

    classifyAnomaly(out);

    auto t1 = std::chrono::steady_clock::now();
    out.processingTimeMs = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    out.updateCrc();
    return out;
}

void AnalysisEngine::updateBaseline(const SdfRecord& sdf) {
    const auto& d = sdf.waveformData;
    if (d.empty()) return;
    m_thresholds.baselineRms    = computeRms(d);
    m_thresholds.baselineEnergy = computeEnergy(d);
    m_thresholds.baselineFreq   = computeDominantFrequency(d, sdf.samplingRate);
}

} // namespace tp
