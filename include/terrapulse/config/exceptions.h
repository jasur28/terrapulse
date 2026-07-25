#pragma once

#include "terrapulse/core/exceptions.h"

#include <string>

namespace tp::config {

class Exception : public tp::core::Exception {
public:
    explicit Exception(const std::string& what = "configuration exception")
        : tp::core::Exception(what) {}
};

class OptionNotFoundException : public Exception {
public:
    explicit OptionNotFoundException(const std::string& name)
        : Exception("option not found: " + name) {}
};

class TypeConversionException : public Exception {
public:
    explicit TypeConversionException(const std::string& value)
        : Exception("type conversion error: " + value) {}
};

class SyntaxException : public Exception {
public:
    explicit SyntaxException(const std::string& message)
        : Exception("syntax error: " + message) {}
};

} // namespace tp::config
