#pragma once

#include "terrapulse/core/datetime.h"

#include <deque>
#include <list>
#include <string>

namespace tp::client {

class RunningAverage {
public:
    explicit RunningAverage(int timeSpanSeconds = 60) : m_timeSpan(timeSpanSeconds) {}

    int timeSpan() const { return m_timeSpan; }
    void push(const tp::core::Time& time, std::size_t count = 1);
    int count(const tp::core::Time& time) const;
    double value(const tp::core::Time& time) const;
    tp::core::Time last() const { return m_last; }

private:
    int m_timeSpan = 60;
    tp::core::Time m_last;
    std::deque<std::pair<qint64, std::size_t>> m_bins;
};

class ObjectMonitor {
public:
    using Log = RunningAverage;

    explicit ObjectMonitor(int timeSpanSeconds = 60) : m_timeSpan(timeSpanSeconds) {}

    Log* add(const std::string& name, const std::string& channel = {});
    void update(const tp::core::Time& time);

    struct Test {
        std::string name;
        std::string channel;
        tp::core::Time updateTime;
        std::size_t count = 0;
        Log log;
    };

    using Tests = std::list<Test>;
    Tests::const_iterator begin() const { return m_tests.begin(); }
    Tests::const_iterator end() const { return m_tests.end(); }
    std::size_t size() const { return m_tests.size(); }

private:
    Tests m_tests;
    int m_timeSpan = 60;
};

} // namespace tp::client
