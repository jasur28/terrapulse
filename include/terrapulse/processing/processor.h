#pragma once

#include "terrapulse/core/datetime.h"

#include <QString>

#include <utility>

namespace tp::processing {

enum class ProcessorStatus {
    Waiting,
    InProgress,
    Finished,
    Error
};

class Processor {
public:
    virtual ~Processor() = default;

    ProcessorStatus status() const { return m_status; }
    QString error() const { return m_error; }
    tp::core::Time lastUpdate() const { return m_lastUpdate; }

    virtual void reset() {
        m_status = ProcessorStatus::Waiting;
        m_error.clear();
        m_lastUpdate = tp::core::Time::now();
    }

protected:
    void setStatus(ProcessorStatus value) {
        m_status = value;
        m_lastUpdate = tp::core::Time::now();
    }

    void setError(QString error) {
        m_error = std::move(error);
        setStatus(ProcessorStatus::Error);
    }

private:
    ProcessorStatus m_status = ProcessorStatus::Waiting;
    QString m_error;
    tp::core::Time m_lastUpdate = tp::core::Time::now();
};

} // namespace tp::processing
