#pragma once

#include "terrapulse/core/record.h"
#include "terrapulse/datamodel/waveformquality.h"
#include "terrapulse/math/filter.h"
#include "terrapulse/processing/waveformprocessor.h"

#include <QString>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace tp::qc {

class QcProcessor : public tp::processing::WaveformProcessor {
public:
    explicit QcProcessor(QString parameter = {}) : m_parameter(std::move(parameter)) {}
    QString parameter() const { return m_parameter; }
    const std::vector<std::shared_ptr<tp::datamodel::WaveformQuality>>& results() const { return m_results; }

    void reset() override {
        tp::processing::WaveformProcessor::reset();
        m_results.clear();
    }

protected:
    std::shared_ptr<tp::datamodel::WaveformQuality> makeQuality(const tp::core::Record& record, double value) {
        auto quality = std::make_shared<tp::datamodel::WaveformQuality>();
        quality->waveformID.networkCode = QString::fromStdString(record.networkCode());
        quality->waveformID.stationCode = QString::fromStdString(record.stationCode());
        quality->waveformID.locationCode = QString::fromStdString(record.locationCode());
        quality->waveformID.channelCode = QString::fromStdString(record.channelCode());
        quality->type = m_parameter;
        quality->parameter = m_parameter;
        quality->value = value;
        quality->start = record.startTime();
        quality->end = record.endTime();
        m_results.push_back(quality);
        setStatus(tp::processing::ProcessorStatus::Finished);
        return quality;
    }

private:
    QString m_parameter;
    std::vector<std::shared_ptr<tp::datamodel::WaveformQuality>> m_results;
};

class LatencyProcessor : public QcProcessor {
public:
    LatencyProcessor() : QcProcessor(QStringLiteral("latency")) {}
    bool feed(const tp::core::Record& record) override {
        makeQuality(record, (tp::core::Time::now() - record.endTime()).length());
        return true;
    }
};

class RmsProcessor : public QcProcessor {
public:
    RmsProcessor() : QcProcessor(QStringLiteral("rms")) {}
    bool feed(const tp::core::Record& record) override {
        const auto* array = dynamic_cast<const tp::core::DoubleArray*>(record.data());
        makeQuality(record, array ? tp::math::rms(array->values()) : 0.0);
        return true;
    }
};

class SpikeProcessor : public QcProcessor {
public:
    explicit SpikeProcessor(double threshold = 5.0) : QcProcessor(QStringLiteral("spike")), m_threshold(threshold) {}
    bool feed(const tp::core::Record& record) override {
        const auto* array = dynamic_cast<const tp::core::DoubleArray*>(record.data());
        int count = 0;
        if (array) {
            const double baseline = tp::math::rms(array->values());
            const double limit = std::max(m_threshold * baseline, 1e-12);
            for (double value : array->values()) {
                if (std::abs(value) > limit) {
                    ++count;
                }
            }
        }
        makeQuality(record, count);
        return true;
    }

private:
    double m_threshold;
};

class GapProcessor : public QcProcessor {
public:
    GapProcessor() : QcProcessor(QStringLiteral("gap")) {}
    bool feed(const tp::core::Record& record) override {
        double gap = 0.0;
        if (m_havePrevious) {
            gap = std::max(0.0, (record.startTime() - m_previousEnd).length());
        }
        m_previousEnd = record.endTime();
        m_havePrevious = true;
        makeQuality(record, gap);
        return true;
    }

private:
    tp::core::Time m_previousEnd;
    bool m_havePrevious = false;
};

class AvailabilityProcessor : public QcProcessor {
public:
    explicit AvailabilityProcessor(double expectedRate = 100.0)
        : QcProcessor(QStringLiteral("availability")), m_expectedRate(expectedRate) {}

    bool feed(const tp::core::Record& record) override {
        const double duration = std::max(1e-12, (record.endTime() - record.startTime()).length());
        const double expected = duration * m_expectedRate;
        const double availability = expected <= 0.0 ? 0.0 : std::min(100.0, 100.0 * record.sampleCount() / expected);
        makeQuality(record, availability);
        return true;
    }

private:
    double m_expectedRate;
};

} // namespace tp::qc
