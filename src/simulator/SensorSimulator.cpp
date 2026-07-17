#include "simulator/SensorSimulator.h"
#include <QDateTime>
#include <cmath>

namespace tp {

static constexpr float PI = 3.14159265358979f;

SensorSimulator::SensorSimulator(const SimulatorConfig& cfg)
    : m_cfg(cfg)
    , m_rng(std::random_device{}())
    , m_noise(0.0f, cfg.noiseLevel)
{
    m_currentTime = QDateTime::currentMSecsSinceEpoch();
}

SdfRecord SensorSimulator::nextWindow() {
    int samples = (m_cfg.samplingRate * m_cfg.windowMs) / 1000;

    SdfRecord rec;
    rec.stationId   = m_cfg.stationId;
    rec.objectId    = m_cfg.objectId;
    rec.sensorId    = m_cfg.sensorId;
    rec.component   = m_cfg.component;
    rec.samplingRate = m_cfg.samplingRate;
    rec.unit        = MeasurementUnit::Millig;
    rec.startTime   = m_currentTime;
    rec.endTime     = m_currentTime + m_cfg.windowMs;
    rec.sampleCount = static_cast<uint32_t>(samples);
    rec.sensorStatus = SensorStatus::Ok;
    rec.temperature  = 23.0f + m_noise(m_rng) * 0.2f;
    rec.batteryLevel = 95;

    switch (m_cfg.scenario) {
    case SimulationScenario::Normal:     rec.waveformData = generateNormal(samples);     break;
    case SimulationScenario::Vibration:  rec.waveformData = generateVibration(samples);  break;
    case SimulationScenario::Resonance:  rec.waveformData = generateResonance(samples);  break;
    case SimulationScenario::Crack:      rec.waveformData = generateCrack(samples);      break;
    case SimulationScenario::Settlement: rec.waveformData = generateSettlement(samples); break;
    case SimulationScenario::Overload:   rec.waveformData = generateOverload(samples);   break;
    }

    rec.sampleCount = static_cast<uint32_t>(rec.waveformData.size());
    rec.updateCrc();
    m_currentTime += m_cfg.windowMs;
    return rec;
}

std::vector<float> SensorSimulator::generateNormal(int samples) {
    std::vector<float> data(samples);
    for (int i = 0; i < samples; ++i)
        data[i] = m_noise(m_rng);
    return data;
}

std::vector<float> SensorSimulator::generateVibration(int samples) {
    std::vector<float> data(samples);
    float freq = 8.0f; // Hz structural vibration
    float amp  = 12.0f; // mg
    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / m_cfg.samplingRate;
        data[i] = amp * std::sin(2.0f * PI * freq * t) + m_noise(m_rng) * 2.0f;
    }
    return data;
}

std::vector<float> SensorSimulator::generateResonance(int samples) {
    std::vector<float> data(samples);
    float freq = 20.0f; // Hz — shifted from baseline ~5 Hz
    float amp  = 25.0f;
    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / m_cfg.samplingRate;
        data[i] = amp * std::sin(2.0f * PI * freq * t) +
                  amp * 0.3f * std::sin(2.0f * PI * freq * 2.0f * t) +
                  m_noise(m_rng);
    }
    return data;
}

std::vector<float> SensorSimulator::generateCrack(int samples) {
    std::vector<float> data(samples);
    // Background low-freq vibration + high-freq impulse burst.
    float bgFreq  = 5.0f;
    float bgAmp   = 3.0f;
    float burstFreq = 60.0f; // high-freq crack signature
    float burstAmp  = 55.0f;
    int   burstStart = samples / 3;
    int   burstLen   = samples / 10;
    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / m_cfg.samplingRate;
        float v = bgAmp * std::sin(2.0f * PI * bgFreq * t) + m_noise(m_rng);
        if (i >= burstStart && i < burstStart + burstLen) {
            float envelope = std::exp(-5.0f * static_cast<float>(i - burstStart) / burstLen);
            v += burstAmp * envelope * std::sin(2.0f * PI * burstFreq * t);
        }
        data[i] = v;
    }
    return data;
}

std::vector<float> SensorSimulator::generateSettlement(int samples) {
    std::vector<float> data(samples);
    // Slowly drifting low-frequency component (settlement signature).
    float baseFreq  = 2.0f;
    float driftHz   = 0.5f; // drift per window
    float amp       = 8.0f;
    for (int i = 0; i < samples; ++i) {
        float t    = static_cast<float>(i) / m_cfg.samplingRate;
        float freq = baseFreq + driftHz * (static_cast<float>(i) / samples);
        data[i] = amp * std::sin(2.0f * PI * freq * t) + m_noise(m_rng);
    }
    return data;
}

std::vector<float> SensorSimulator::generateOverload(int samples) {
    std::vector<float> data(samples);
    float amp = 130.0f; // mg — above critical 100 mg
    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / m_cfg.samplingRate;
        data[i] = amp * std::sin(2.0f * PI * 5.0f * t) + m_noise(m_rng) * 3.0f;
    }
    return data;
}

} // namespace tp
