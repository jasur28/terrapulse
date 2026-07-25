#pragma once

#include "terrapulse/core/array.h"
#include "terrapulse/core/baseobject.h"
#include "terrapulse/core/interruptible.h"
#include "terrapulse/core/record.h"
#include "terrapulse/core/timewindow.h"

#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace tp::io {

class RecordStream : public tp::core::BaseObject, public tp::core::Interruptible {
public:
    using Ptr = std::unique_ptr<RecordStream>;
    using Factory = std::function<Ptr()>;

    ~RecordStream() override = default;

    virtual bool setSource(const QString& source) = 0;
    virtual void close() = 0;
    virtual bool addStream(const QString& networkCode, const QString& stationCode,
                           const QString& locationCode, const QString& channelCode) = 0;
    virtual bool addStream(const QString& networkCode, const QString& stationCode,
                           const QString& locationCode, const QString& channelCode,
                           std::optional<tp::core::Time> startTime,
                           std::optional<tp::core::Time> endTime) = 0;
    virtual bool setStartTime(std::optional<tp::core::Time> startTime);
    virtual bool setEndTime(std::optional<tp::core::Time> endTime);
    virtual bool setTimeWindow(const tp::core::TimeWindow& timeWindow);
    virtual bool setTimeout(int seconds);
    virtual bool setRecordType(const QString& type);
    virtual std::unique_ptr<tp::core::Record> next() = 0;

    void setDataType(tp::core::Array::DataType dataType) { m_dataType = dataType; }
    void setDataHint(tp::core::Record::Hint hint) { m_hint = hint; }
    tp::core::Array::DataType dataType() const { return m_dataType; }
    tp::core::Record::Hint dataHint() const { return m_hint; }

    static bool RegisterFactory(const QString& service, Factory factory);
    static Ptr Create(const QString& service);
    static Ptr Open(const QString& url);

protected:
    void setupRecord(tp::core::Record* record) const;

    std::optional<tp::core::Time> m_startTime;
    std::optional<tp::core::Time> m_endTime;
    int m_timeoutSeconds = 0;
    QString m_recordType = "mseed";
    tp::core::Array::DataType m_dataType = tp::core::Array::DataType::Double;
    tp::core::Record::Hint m_hint = tp::core::Record::Hint::SaveRaw;
};

} // namespace tp::io
