#include "history/HistoryEngine.h"
#include <algorithm>
#include <optional>

namespace tp {

uint64_t HistoryEngine::makeKey(uint32_t sensorId, Axis axis) const {
    return (static_cast<uint64_t>(sensorId) << 8) | static_cast<uint64_t>(axis);
}

SeverityLevel HistoryEngine::computeSeverity(WarningLevel wl, float health) const {
    if (wl == WarningLevel::Critical || health < 0.3f) return SeverityLevel::Critical;
    if (wl == WarningLevel::Warning  || health < 0.6f) {
        return (health < 0.45f) ? SeverityLevel::High : SeverityLevel::Medium;
    }
    return SeverityLevel::Low;
}

Trend HistoryEngine::computeTrend(const ShfRecord& event) const {
    const auto& tl = event.timeline;
    if (tl.size() < 3) return Trend::Unknown;
    // Use last 3 health snapshots stored in timeline (not directly available,
    // so we use growthRate sign as a proxy).
    if (event.growthRate > 0.05f) return Trend::Worsening;
    if (event.growthRate < -0.05f) return Trend::Improving;
    return Trend::Stable;
}

ShfRecord HistoryEngine::openEvent(const SafRecord& saf) {
    ShfRecord ev;
    ev.shfId            = m_nextShfId++;
    ev.sourceSaf        = saf.safId;
    ev.objectId         = saf.objectId;
    ev.sensorId         = saf.sensorId;
    ev.component        = saf.component;
    ev.anomalyType      = saf.anomalyType;
    ev.anomalyStartTime = saf.analysisStartTime;
    ev.anomalyEndTime   = 0; // ongoing
    ev.anomalyStatus    = AnomalyStatus::Active;
    ev.anomalyDuration  = 0;
    ev.maxValue         = saf.maxAmplitude;
    ev.averageValue     = saf.rms;
    ev.growthRate       = 0.0f;
    ev.severityLevel    = computeSeverity(saf.warningLevel, saf.healthIndex);
    ev.confidenceLevel  = saf.confidenceLevel;
    ev.trend            = Trend::Unknown;
    ev.sampleCount      = 1;
    ev.timeline.push_back({saf.safId, saf.analysisStartTime});
    ev.updateCrc();
    return ev;
}

void HistoryEngine::updateEvent(ShfRecord& ev, const SafRecord& saf) {
    ev.anomalyEndTime  = saf.analysisEndTime;
    ev.anomalyDuration = static_cast<uint32_t>(
        (saf.analysisEndTime - ev.anomalyStartTime) / 1000);

    // Running max and average.
    ev.maxValue     = std::max(ev.maxValue, saf.maxAmplitude);
    float n         = static_cast<float>(ev.sampleCount);
    ev.averageValue = (ev.averageValue * n + saf.rms) / (n + 1.0f);
    ++ev.sampleCount;

    // Growth rate: Δ max per hour.
    float hours = static_cast<float>(ev.anomalyDuration) / 3600.0f;
    ev.growthRate = (hours > 0.0f) ? (ev.maxValue / hours) : 0.0f;

    ev.severityLevel   = computeSeverity(saf.warningLevel, saf.healthIndex);
    ev.confidenceLevel = std::max(ev.confidenceLevel, saf.confidenceLevel);
    ev.trend           = computeTrend(ev);
    ev.timeline.push_back({saf.safId, saf.analysisStartTime});
    ev.updateCrc();
}

std::optional<ShfRecord> HistoryEngine::processSaf(const SafRecord& saf) {
    uint64_t key = makeKey(saf.sensorId, saf.component);

    if (saf.anomalyDetected) {
        auto it = m_activeIndex.find(key);
        if (it == m_activeIndex.end()) {
            // Open new event.
            m_activeEvents.push_back(openEvent(saf));
            m_activeIndex[key] = m_activeEvents.size() - 1;
            return m_activeEvents.back();
        } else {
            // Update existing event.
            updateEvent(m_activeEvents[it->second], saf);
            return m_activeEvents[it->second];
        }
    } else {
        // No anomaly — close active event for this sensor/axis if any.
        auto it = m_activeIndex.find(key);
        if (it != m_activeIndex.end()) {
            ShfRecord& ev     = m_activeEvents[it->second];
            ev.anomalyEndTime = saf.analysisEndTime;
            ev.anomalyStatus  = AnomalyStatus::Resolved;
            ev.trend          = computeTrend(ev);
            ev.updateCrc();
            ShfRecord closed = ev;
            m_resolvedEvents.push_back(closed);
            // Remove from active list.
            m_activeEvents.erase(m_activeEvents.begin() + static_cast<ptrdiff_t>(it->second));
            m_activeIndex.erase(it);
            // Rebuild index.
            m_activeIndex.clear();
            for (size_t i = 0; i < m_activeEvents.size(); ++i)
                m_activeIndex[makeKey(m_activeEvents[i].sensorId, m_activeEvents[i].component)] = i;
            return closed;
        }
        return std::nullopt;
    }
}

} // namespace tp
