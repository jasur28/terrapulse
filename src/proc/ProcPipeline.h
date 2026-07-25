#pragma once
#include "analysis/AnalysisEngine.h"
#include "history/HistoryEngine.h"
#include "core/SdfRecord.h"
#include "core/SafRecord.h"
#include "core/ShfRecord.h"
#include <QVariantMap>
#include <QString>
#include <deque>
#include <functional>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <unordered_map>

namespace tp {

// ProcPipeline — turns a stream of raw 3-axis samples into SAF/SHF results.
// Buffers samples into windows, runs AnalysisEngine + HistoryEngine, and hands
// the results out as QVariantMaps (ready for the bus / UI). Lifted out of the
// old in-app AppController so the tpproc daemon can own the analysis.
class ProcPipeline {
public:
    using MapCallback = std::function<void(const QVariantMap&)>;

    ProcPipeline() = default;

    void setWindowSize(int n) { if (n >= 4) m_windowSize = n; }
    int  windowSize() const { return m_windowSize; }
    int  windowsProcessed() const { return m_windowsProcessed; }

    // Samples used for the natural-frequency estimate (see m_freqBuf).
    void setFreqWindow(int n) { if (n >= 128) m_freqBufLen = std::size_t(n); }

    // Base thresholds applied to every new sensor-axis (from configuration).
    // Each axis then learns its own baseline on top of these.
    void setThresholds(const AnalysisThresholds& t) { m_baseThresholds = t; }

    // Per-sensor thresholds from a binding profile — one structure can be judged
    // by different limits than another. Applied when that sensor is first seen.
    void setThresholdsFor(uint32_t object, uint32_t sensor, const AnalysisThresholds& t) {
        m_sensorThresholds[(uint64_t(object) << 32) | sensor] = t;
    }
    bool hasThresholdsFor(uint32_t object, uint32_t sensor) const {
        return m_sensorThresholds.count((uint64_t(object) << 32) | sensor) > 0;
    }

    // Windows of quiet startup used to learn the baseline.
    void setCalibrationWindows(int n) { if (n >= 1) m_calibWindows = n; }

    // Band searched for the dominant/natural frequency (Hz); fMax <= 0 = Nyquist.
    void setFreqBand(double fMin, double fMax) { m_freqMin = fMin; m_freqMax = fMax; }

    // STA/LTA onset gate (hysteresis). on/off are ratio thresholds.
    void setStaLta(double staAlpha, double ltaAlpha, double onRatio, double offRatio) {
        if (staAlpha > 0) m_staAlpha = staAlpha;
        if (ltaAlpha > 0) m_ltaAlpha = ltaAlpha;
        if (onRatio  > 0) m_onRatio  = onRatio;
        if (offRatio > 0) m_offRatio = offRatio;
    }

    // Feed one raw sample. Identity travels with each sample so a single
    // pipeline can be retargeted, though typically one pipeline == one sensor.
    void addSample(uint32_t station, uint32_t object, uint32_t sensor,
                   double x, double y, double z,
                   int64_t tMs, uint32_t sampleRate);

    MapCallback onSaf;
    MapCallback onShf;

private:
    void flushWindow();
    QVariantMap toVariant(const SafRecord& s) const;
    QVariantMap toVariant(const ShfRecord& s) const;

    static QString axisName(Axis a);
    static QString anomalyName(AnomalyType t);
    static QString warningName(WarningLevel l);
    static QString warningColor(WarningLevel l);
    static QString severityName(SeverityLevel s);
    static QString severityColor(SeverityLevel s);
    static QString trendName(Trend t);
    static QString statusName(AnomalyStatus s);

    // Per-sensor-axis baseline (the structure's "normal"), learned from the first
    // kCalibWindows windows. Health and anomalies are judged relative to it.
    struct BaselineState {
        AnalysisThresholds th;                 // thresholds with a calibrated baseline
        double sumRms = 0.0, sumEnergy = 0.0, sumFreq = 0.0;
        int    count = 0;
        bool   ready = false;
    };
    std::unordered_map<uint64_t, BaselineState> m_baseline;
    std::unordered_map<uint64_t, AnalysisThresholds> m_sensorThresholds;  // binding profiles
    AnalysisThresholds m_baseThresholds;       // from configuration
    int    m_calibWindows = 20;                // ~10 s of "quiet" startup
    double m_freqMin = 0.3, m_freqMax = 0.0;   // structural band (Hz)

    // Per-sensor-axis STA/LTA onset detector: short- vs long-term average of the
    // window RMS (characteristic function). Triggers with hysteresis (on>off) so
    // a sustained energy rise — not a single-window spike — confirms an anomaly.
    struct TriggerState {
        double sta = 0.0, lta = 0.0;
        bool   primed = false;
        bool   triggered = false;
    };
    std::unordered_map<uint64_t, TriggerState> m_trigger;

    // Natural (modal) frequency needs finer resolution than the detection window
    // gives: a 0.5 s window resolves only ~2 Hz, far too coarse to see the small
    // drift that signals stiffness loss. So detection stays on the short window
    // (fast reaction) while the frequency is estimated from a longer rolling
    // buffer per axis (finer bins), and overrides the per-window value.
    std::unordered_map<uint64_t, std::deque<float>> m_freqBuf;
    std::size_t m_freqBufLen = 1024;           // ~5 s at 200 Hz -> ~0.2 Hz bins
    double m_staAlpha = 0.34;   // EMA ~ last 3 windows (short-term)
    double m_ltaAlpha = 0.02;   // EMA ~ last 50 windows (long-term background)
    double m_onRatio  = 3.0;    // STA/LTA to open a trigger
    double m_offRatio = 1.5;    // STA/LTA to close it (hysteresis)

    AnalysisEngine m_analysis;
    HistoryEngine  m_history;

    int      m_windowSize       = 100;
    int      m_windowsProcessed = 0;
    uint64_t m_safCounter       = 1;

    std::vector<float> m_bufX, m_bufY, m_bufZ;
    int64_t  m_windowStartMs = 0;
    int64_t  m_windowEndMs   = 0;
    uint32_t m_samplingRate  = 100;
    uint32_t m_station = 1, m_object = 1, m_sensor = 1;
};

} // namespace tp
