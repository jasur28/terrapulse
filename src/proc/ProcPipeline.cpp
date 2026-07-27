#include "proc/ProcPipeline.h"
#include "analysis/Spectrum.h"

namespace tp {

void ProcPipeline::addSample(uint32_t station, uint32_t object, uint32_t sensor,
                             double x, double y, double z,
                             int64_t tMs, uint32_t sampleRate) {
    Window& w = m_windows[(static_cast<uint64_t>(object) << 32) | sensor];
    if (w.x.empty()) {
        w.startMs = tMs;
        w.rate    = sampleRate > 0 ? sampleRate : 100;
        w.station = station;
        w.object  = object;
        w.sensor  = sensor;
    }
    w.x.push_back(static_cast<float>(x));
    w.y.push_back(static_cast<float>(y));
    w.z.push_back(static_cast<float>(z));
    w.endMs = tMs;

    if (static_cast<int>(w.x.size()) >= m_windowSize)
        flushWindow(w);
}

void ProcPipeline::flushWindow(Window& w) {
    ++m_windowsProcessed;

    auto makeSdf = [&](Axis axis, const std::vector<float>& data) {
        SdfRecord sdf;
        sdf.stationId    = w.station;
        sdf.objectId     = w.object;
        sdf.sensorId     = w.sensor;
        sdf.component    = axis;
        sdf.samplingRate = w.rate;
        sdf.startTime    = w.startMs;
        sdf.endTime      = w.endMs;
        sdf.waveformData = data;
        sdf.sampleCount  = static_cast<uint32_t>(data.size());
        sdf.updateCrc();
        return sdf;
    };

    struct AxisEntry { Axis axis; const std::vector<float>& buf; };
    const AxisEntry axes[] = {
        {Axis::X, w.x},
        {Axis::Y, w.y},
        {Axis::Z, w.z}
    };

    for (const auto& a : axes) {
        SdfRecord sdf = makeSdf(a.axis, a.buf);

        // Judge this axis against ITS OWN learned baseline, starting from the
        // configured thresholds.
        const uint64_t key = (static_cast<uint64_t>(w.object) << 40)
                           | (static_cast<uint64_t>(w.sensor) << 8)
                           | static_cast<uint64_t>(a.axis);
        auto bIt = m_baseline.find(key);
        if (bIt == m_baseline.end()) {
            BaselineState fresh;
            // A binding profile for this sensor wins over the global thresholds.
            const auto sIt = m_sensorThresholds.find((uint64_t(w.object) << 32) | w.sensor);
            fresh.th = sIt != m_sensorThresholds.end() ? sIt->second : m_baseThresholds;
            bIt = m_baseline.emplace(key, fresh).first;
        }
        BaselineState& b = bIt->second;
        m_analysis.setThresholds(b.th);

        SafRecord saf = m_analysis.analyse(sdf, m_safCounter++);

        // ── Natural frequency from the long rolling buffer (finer bins) ──
        auto& fb = m_freqBuf[key];
        fb.insert(fb.end(), a.buf.begin(), a.buf.end());
        while (fb.size() > m_freqBufLen) fb.pop_front();
        if (fb.size() >= 256) {
            const std::vector<float> fv(fb.begin(), fb.end());
            const double f = spec::dominantFrequency(fv, w.rate, m_freqMin, m_freqMax);
            if (f > 0.0) {
                saf.dominantFrequency = static_cast<float>(f);
                saf.frequencyShift    = b.th.baselineFreq > 0.0f
                                          ? static_cast<float>(f) - b.th.baselineFreq : 0.0f;
            }
        }

        // ── STA/LTA onset tracking (characteristic function = window RMS) ──
        TriggerState& tr = m_trigger[key];
        const double cf = saf.rms;
        if (!tr.primed) { tr.sta = tr.lta = cf; tr.primed = true; }
        tr.sta += m_staAlpha * (cf - tr.sta);
        if (!tr.triggered)                                 // freeze LTA during a trigger
            tr.lta += m_ltaAlpha * (cf - tr.lta);
        const double ratio = tr.lta > 1e-9 ? tr.sta / tr.lta : 1.0;
        if (tr.triggered) { if (ratio < m_offRatio) tr.triggered = false; }
        else              { if (ratio > m_onRatio)  tr.triggered = true;  }

        if (!b.ready) {
            // Calibration period: accumulate the "normal" AC features, don't alarm.
            b.sumRms    += saf.rms;
            b.sumEnergy += saf.energy;
            b.sumFreq   += saf.dominantFrequency;
            if (++b.count >= m_calibWindows) {
                b.th.baselineRms    = static_cast<float>(b.sumRms    / b.count);
                b.th.baselineEnergy = static_cast<float>(b.sumEnergy / b.count);
                b.th.baselineFreq   = static_cast<float>(b.sumFreq   / b.count);
                b.ready = true;
            }
            saf.anomalyDetected = false;
            saf.anomalyType     = AnomalyType::None;
            saf.warningLevel    = WarningLevel::Normal;
            saf.healthIndex     = 1.0f;
            tr.triggered        = false;                   // no alarms while learning
        } else if (saf.anomalyDetected && !tr.triggered) {
            // Energy/vibration onsets must be confirmed by STA/LTA (cuts single-
            // window false positives). Hard absolute overloads alarm regardless.
            const bool hardOverload = saf.warningLevel == WarningLevel::Critical &&
                                      saf.anomalyType  == AnomalyType::Overload;
            if (!hardOverload) {
                saf.anomalyDetected = false;
                saf.anomalyType     = AnomalyType::None;
                saf.warningLevel    = WarningLevel::Normal;
                // health left as computed — features still reflect the elevation
            }
        }

        QVariantMap sv = toVariant(saf);
        sv["staLtaRatio"] = ratio;
        sv["triggered"]   = tr.triggered;
        if (onSaf) onSaf(sv);

        auto shfOpt = m_history.processSaf(saf);
        if (shfOpt.has_value() && onShf) {
            QVariantMap shv = toVariant(*shfOpt);
            shv["stationId"] = static_cast<quint32>(w.station);
            onShf(shv);
        }
    }

    w.x.clear();
    w.y.clear();
    w.z.clear();
}

// ── Variant conversion ─────────────────────────────────────────────────────

QVariantMap ProcPipeline::toVariant(const SafRecord& s) const {
    QVariantMap m;
    m["stationId"]         = static_cast<quint32>(s.stationId);
    m["timestamp"]         = static_cast<qlonglong>(s.analysisStartTime);
    m["objectId"]          = static_cast<quint32>(s.objectId);
    m["sensorId"]          = static_cast<quint32>(s.sensorId);
    m["component"]         = static_cast<int>(s.component);
    m["componentName"]     = axisName(s.component);
    m["rms"]               = static_cast<double>(s.rms);
    m["maxAmplitude"]      = static_cast<double>(s.maxAmplitude);
    m["dominantFrequency"] = static_cast<double>(s.dominantFrequency);
    m["healthIndex"]       = static_cast<double>(s.healthIndex);
    m["anomalyDetected"]   = s.anomalyDetected;
    m["anomalyType"]       = static_cast<int>(s.anomalyType);
    m["anomalyTypeName"]   = anomalyName(s.anomalyType);
    m["warningLevel"]      = static_cast<int>(s.warningLevel);
    m["warningLevelName"]  = warningName(s.warningLevel);
    m["warningLevelColor"] = warningColor(s.warningLevel);
    m["confidenceLevel"]   = static_cast<double>(s.confidenceLevel);
    return m;
}

QVariantMap ProcPipeline::toVariant(const ShfRecord& s) const {
    QVariantMap m;
    m["stationId"]        = static_cast<quint32>(s.objectId);  // caller overrides with the window's station
    m["shfId"]            = static_cast<qulonglong>(s.shfId);
    m["objectId"]         = static_cast<quint32>(s.objectId);
    m["sensorId"]         = static_cast<quint32>(s.sensorId);
    m["component"]        = static_cast<int>(s.component);
    m["componentName"]    = axisName(s.component);
    m["anomalyType"]      = static_cast<int>(s.anomalyType);
    m["anomalyTypeName"]  = anomalyName(s.anomalyType);
    m["anomalyStartTime"] = static_cast<qlonglong>(s.anomalyStartTime);
    m["anomalyEndTime"]   = static_cast<qlonglong>(s.anomalyEndTime);
    m["anomalyStatus"]    = static_cast<int>(s.anomalyStatus);
    m["statusName"]       = statusName(s.anomalyStatus);
    m["anomalyDuration"]  = static_cast<quint32>(s.anomalyDuration);
    m["severityLevel"]    = static_cast<int>(s.severityLevel);
    m["severityName"]     = severityName(s.severityLevel);
    m["severityColor"]    = severityColor(s.severityLevel);
    m["maxValue"]         = static_cast<double>(s.maxValue);
    m["growthRate"]       = static_cast<double>(s.growthRate);
    m["trend"]            = static_cast<int>(s.trend);
    m["trendName"]        = trendName(s.trend);
    m["confidenceLevel"]  = static_cast<double>(s.confidenceLevel);
    return m;
}

// ── String helpers ─────────────────────────────────────────────────────────

QString ProcPipeline::axisName(Axis a) {
    switch (a) {
    case Axis::X: return "X";
    case Axis::Y: return "Y";
    default:      return "Z";
    }
}
QString ProcPipeline::anomalyName(AnomalyType t) {
    switch (t) {
    case AnomalyType::Vibration:  return "Vibration";
    case AnomalyType::Resonance:  return "Resonance";
    case AnomalyType::Crack:      return "Crack";
    case AnomalyType::Settlement: return "Settlement";
    case AnomalyType::Overload:   return "Overload";
    default:                      return "None";
    }
}
QString ProcPipeline::warningName(WarningLevel l) {
    switch (l) {
    case WarningLevel::Critical: return "CRITICAL";
    case WarningLevel::Warning:  return "WARNING";
    default:                     return "NORMAL";
    }
}
QString ProcPipeline::warningColor(WarningLevel l) {
    switch (l) {
    case WarningLevel::Critical: return "#FF1744";
    case WarningLevel::Warning:  return "#FFD600";
    default:                     return "#00C853";
    }
}
QString ProcPipeline::severityName(SeverityLevel s) {
    switch (s) {
    case SeverityLevel::Critical: return "CRITICAL";
    case SeverityLevel::High:     return "HIGH";
    case SeverityLevel::Medium:   return "MEDIUM";
    default:                      return "LOW";
    }
}
QString ProcPipeline::severityColor(SeverityLevel s) {
    switch (s) {
    case SeverityLevel::Critical: return "#FF1744";
    case SeverityLevel::High:     return "#FF6D00";
    case SeverityLevel::Medium:   return "#FFD600";
    default:                      return "#00C853";
    }
}
QString ProcPipeline::trendName(Trend t) {
    switch (t) {
    case Trend::Improving: return "Improving";
    case Trend::Worsening: return "Worsening";
    case Trend::Stable:    return "Stable";
    default:               return "Unknown";
    }
}
QString ProcPipeline::statusName(AnomalyStatus s) {
    switch (s) {
    case AnomalyStatus::Active:     return "ACTIVE";
    case AnomalyStatus::Resolved:   return "RESOLVED";
    case AnomalyStatus::Monitoring: return "MONITORING";
    default:                        return "UNKNOWN";
    }
}

} // namespace tp
