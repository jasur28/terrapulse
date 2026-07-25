#pragma once

#include <stdexcept>
#include <string>

namespace tp::core {

class Exception : public std::runtime_error {
public:
    explicit Exception(const std::string& what) : std::runtime_error(what) {}
};

class TypeException : public Exception {
public:
    explicit TypeException(const std::string& what) : Exception(what) {}
};

class ValueException : public Exception {
public:
    explicit ValueException(const std::string& what) : Exception(what) {}
};

class PropertyNotFoundException : public Exception {
public:
    explicit PropertyNotFoundException(const std::string& name) : Exception("property not found: " + name) {}
};

} // namespace tp::core
