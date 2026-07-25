#pragma once

#include <stdexcept>
#include <string>

namespace tp::io {

class RecordStreamError : public std::runtime_error {
public:
    explicit RecordStreamError(const std::string& message) : std::runtime_error(message) {}
};

class RecordStreamTimeout : public RecordStreamError {
public:
    explicit RecordStreamTimeout(const std::string& message) : RecordStreamError(message) {}
};

class RecordStreamNotFound : public RecordStreamError {
public:
    explicit RecordStreamNotFound(const std::string& message) : RecordStreamError(message) {}
};

} // namespace tp::io
