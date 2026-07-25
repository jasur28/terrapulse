#pragma once

#include "terrapulse/io/recordstream.h"

#include <iterator>

namespace tp::io {

class RecordInput;

class RecordIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = tp::core::Record*;
    using difference_type = std::ptrdiff_t;
    using pointer = tp::core::Record**;
    using reference = tp::core::Record*&;

    RecordIterator() = default;
    RecordIterator(const RecordIterator& other);
    RecordIterator& operator=(const RecordIterator& other);
    tp::core::Record* operator*() const { return m_current.get(); }
    RecordIterator& operator++();
    RecordIterator operator++(int);
    bool operator==(const RecordIterator& other) const;
    bool operator!=(const RecordIterator& other) const { return !(*this == other); }

private:
    explicit RecordIterator(RecordInput* source);

    RecordInput* m_source = nullptr;
    std::unique_ptr<tp::core::Record> m_current;

    friend class RecordInput;
};

class RecordInput {
public:
    explicit RecordInput(RecordStream* stream,
                         tp::core::Array::DataType dataType = tp::core::Array::DataType::Double,
                         tp::core::Record::Hint hint = tp::core::Record::Hint::SaveRaw);

    RecordIterator begin();
    RecordIterator end();
    std::unique_ptr<tp::core::Record> next();

private:
    RecordStream* m_stream = nullptr;
};

} // namespace tp::io
