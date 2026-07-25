#pragma once

#include <QDateTime>
#include <QTimeZone>

#include <chrono>
#include <cstdint>

namespace tp::core {

class TimeSpan {
public:
    using Duration = std::chrono::microseconds;

    TimeSpan() = default;
    explicit TimeSpan(Duration duration) : m_duration(duration) {}
    explicit TimeSpan(double seconds)
        : m_duration(static_cast<std::int64_t>(seconds * 1000000.0)) {}

    static TimeSpan seconds(double seconds) { return TimeSpan(seconds); }

    double length() const { return static_cast<double>(m_duration.count()) / 1000000.0; }
    std::int64_t microseconds() const { return m_duration.count(); }
    Duration duration() const { return m_duration; }

    TimeSpan operator+(const TimeSpan& other) const { return TimeSpan(m_duration + other.m_duration); }
    TimeSpan operator-(const TimeSpan& other) const { return TimeSpan(m_duration - other.m_duration); }
    bool operator<(const TimeSpan& other) const { return m_duration < other.m_duration; }
    bool operator==(const TimeSpan& other) const { return m_duration == other.m_duration; }

private:
    Duration m_duration{0};
};

class Time {
public:
    Time() = default;
    explicit Time(QDateTime value) : m_value(value.toUTC()) {}

    static Time now() { return Time(QDateTime::currentDateTimeUtc()); }
    static Time fromMSecsSinceEpoch(qint64 value) { return Time(QDateTime::fromMSecsSinceEpoch(value, QTimeZone::UTC)); }

    qint64 toMSecsSinceEpoch() const { return m_value.toMSecsSinceEpoch(); }
    QString toString(Qt::DateFormat format = Qt::ISODateWithMs) const { return m_value.toString(format); }
    const QDateTime& qDateTime() const { return m_value; }

    Time operator+(const TimeSpan& span) const {
        return Time(m_value.addMSecs(span.microseconds() / 1000));
    }

    TimeSpan operator-(const Time& other) const {
        return TimeSpan(std::chrono::microseconds((toMSecsSinceEpoch() - other.toMSecsSinceEpoch()) * 1000));
    }

    bool operator<(const Time& other) const { return m_value < other.m_value; }
    bool operator<=(const Time& other) const { return m_value <= other.m_value; }
    bool operator==(const Time& other) const { return m_value == other.m_value; }

private:
    QDateTime m_value = QDateTime::currentDateTimeUtc();
};

} // namespace tp::core
