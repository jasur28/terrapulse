#pragma once
#include "analysis/AnalysisEngine.h"
#include "history/HistoryEngine.h"
#include "core/SdfRecord.h"
#include "core/SafRecord.h"
#include "core/ShfRecord.h"
#include <QVariantMap>
#include <QString>
#include <functional>
#include <vector>
#include <cstdint>

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
