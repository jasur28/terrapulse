#pragma once
#include "core/SdfRecord.h"
#include <cstdint>
#include <random>

namespace tp {

enum class SimulationScenario {
    Normal,
    Vibration,
    Resonance,
    Crack,
    Settlement,
    Overload
};

struct SimulatorConfig {
    uint32_t stationId   = 1;
    uint32_t objectId    = 1;
    uint32_t sensorId    = 1;
    Axis     component   = Axis::Z;
    uint32_t samplingRate = 100; // Hz
    int      windowMs    = 1000; // ms per SDF window
    float    noiseLevel  = 0.5f; // mg background noise amplitude
    SimulationScenario scenario = SimulationScenario::Normal;
};

class SensorSimulator {
public:
    explicit SensorSimulator(const SimulatorConfig& cfg = {});

    // Generate the next SDF window, advancing internal clock.
    SdfRecord nextWindow();

    void setScenario(SimulationScenario s) { m_cfg.scenario = s; }
    void setConfig(const SimulatorConfig& c) { m_cfg = c; }
    const SimulatorConfig& config() const { return m_cfg; }

private:
    SimulatorConfig m_cfg;
    int64_t         m_currentTime = 0;  // Unix ms
    std::mt19937    m_rng;
    std::normal_distribution<float> m_noise;

    std::vector<float> generateNormal(int samples);
    std::vector<float> generateVibration(int samples);
    std::vector<float> generateResonance(int samples);
    std::vector<float> generateCrack(int samples);
    std::vector<float> generateSettlement(int samples);
    std::vector<float> generateOverload(int samples);
};

} // namespace tp
