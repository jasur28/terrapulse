#pragma once

#include "terrapulse/core/array.h"
#include "terrapulse/core/baseobject.h"
#include "terrapulse/core/timewindow.h"

#include <memory>
#include <string>

namespace tp::core {

class Record : public BaseObject {
public:
    enum class Hint {
        MetaOnly,
        DataOnly,
        SaveRaw
    };

    virtual ~Record() = default;

    const std::string& networkCode() const { return m_network; }
    const std::string& stationCode() const { return m_station; }
    const std::string& locationCode() const { return m_location; }
    const std::string& channelCode() const { return m_channel; }
    const Time& startTime() const { return m_startTime; }
    int sampleCount() const { return m_sampleCount; }
    double samplingFrequency() const { return m_samplingFrequency; }
    int timingQuality() const { return m_timingQuality; }

    void setStream(std::string network, std::string station, std::string location, std::string channel);
    void setStartTime(Time time) { m_startTime = time; }
    void setSampling(int count, double frequency);
    void setTimingQuality(int quality) { m_timingQuality = quality; }

    std::string streamID() const;
    Time endTime() const;
    TimeWindow timeWindow() const { return TimeWindow(m_startTime, endTime()); }

    virtual const Array* data() const = 0;
    virtual std::unique_ptr<Record> copy() const = 0;

private:
    std::string m_network;
    std::string m_station;
    std::string m_location;
    std::string m_channel;
    Time m_startTime;
    int m_sampleCount = 0;
    double m_samplingFrequency = 0.0;
    int m_timingQuality = -1;
};

class AccelerationRecord : public Record {
public:
    explicit AccelerationRecord(std::vector<double> samples = {});

    const Array* data() const override { return &m_samples; }
    std::unique_ptr<Record> copy() const override;
    DoubleArray& samples() { return m_samples; }

private:
    DoubleArray m_samples;
};

} // namespace tp::core
