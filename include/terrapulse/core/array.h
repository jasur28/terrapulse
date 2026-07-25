#pragma once

#include "terrapulse/core/baseobject.h"

#include <QByteArray>

#include <memory>
#include <vector>

namespace tp::core {

class Array : public BaseObject {
public:
    enum class DataType {
        Int,
        Float,
        Double,
        DateTime,
        String
    };

    explicit Array(DataType type) : m_type(type) {}
    DataType dataType() const { return m_type; }

    virtual const void* data() const = 0;
    virtual int size() const = 0;
    virtual int elementSize() const = 0;
    virtual void clear() = 0;
    virtual std::unique_ptr<Array> copy(DataType type) const = 0;
    virtual QByteArray bytes() const = 0;

private:
    DataType m_type;
};

template <typename T>
class TypedArray : public Array {
public:
    explicit TypedArray(DataType type) : Array(type) {}
    explicit TypedArray(DataType type, std::vector<T> values) : Array(type), m_values(std::move(values)) {}

    const void* data() const override { return m_values.data(); }
    int size() const override { return static_cast<int>(m_values.size()); }
    int elementSize() const override { return static_cast<int>(sizeof(T)); }
    void clear() override { m_values.clear(); }
    const std::vector<T>& values() const { return m_values; }
    std::vector<T>& values() { return m_values; }

    std::unique_ptr<Array> copy(DataType type) const override {
        if (type == dataType()) return std::make_unique<TypedArray<T>>(type, m_values);
        return {};
    }

    QByteArray bytes() const override {
        return QByteArray(reinterpret_cast<const char*>(m_values.data()),
                          static_cast<qsizetype>(m_values.size() * sizeof(T)));
    }

private:
    std::vector<T> m_values;
};

using IntArray = TypedArray<int>;
using FloatArray = TypedArray<float>;
using DoubleArray = TypedArray<double>;

} // namespace tp::core
