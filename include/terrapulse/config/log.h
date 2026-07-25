#pragma once

#include <string>

namespace tp::config {

enum class LogLevel {
    Error,
    Warning,
    Info,
    Debug
};

class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(LogLevel level, const std::string& file, int line, const std::string& message);
};

} // namespace tp::config
