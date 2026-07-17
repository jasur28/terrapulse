#pragma once
#include "core/SdfRecord.h"
#include "core/SafRecord.h"
#include <cstdint>

namespace tp {

struct AnalysisThresholds {
    // RMS thresholds relative to baseline
    float rmsWarningFactor   = 2.0f;
    float rmsCriticalFactor  = 4.0f;
    // Energy thresholds relative to baseline
    float energyWarningFactor  = 3.0f;
    float energyCriticalFactor = 6.0f;
    // Absolute amplitude thresholds (mg)
    float ampWarning  = 50.0f;
    float ampCritical = 100.0f;
    // Frequency shift thresholds (Hz)
    float freqShiftWarning  = 5.0f;
    float freqShiftCritical = 15.0f;
    // Health index thresholds
    float healthWarning  = 0.6f;
    float healthCritical = 0.3f;

    // Baseline values (set after calibration period)
    float baselineRms    = 1.0f;
    float baselineEnergy = 1.0f;
    float baselineFreq   = 0.0f;  // Hz, 0 = not yet established

    // Health index weights
    float wRms    = 0.35f;
    float wEnergy = 0.25f;
    float wAmp    = 0.25f;
    float wFreq   = 0.15f;
};

class AnalysisEngine {
public:
    explicit AnalysisEngine(const AnalysisThresholds& thresholds = {});

    // Analyse one SDF record and produce a SafRecord.
    // safId must be unique (caller responsibility).
    SafRecord analyse(const SdfRecord& sdf, uint64_t safId) const;

    void setThresholds(const AnalysisThresholds& t) { m_thresholds = t; }
    const AnalysisThresholds& thresholds() const { return m_thresholds; }

    // Update baseline from a "normal" SDF window.
    void updateBaseline(const SdfRecord& sdf);

private:
    AnalysisThresholds m_thresholds;

    static float computeRms(const std::vector<float>& data);
    static float computeVariance(const std::vector<float>& data, float mean);
    static float computeEnergy(const std::vector<float>& data);
    static float computeMaxAmplitude(const std::vector<float>& data);
    static float computeDominantFrequency(const std::vector<float>& data, uint32_t samplingRate);

    float computeHealthIndex(float rms, float energy, float maxAmp, float freqShift) const;
    void classifyAnomaly(SafRecord& out) const;
};

} // namespace tp
