#pragma once

#include <deque>

namespace tp::broker::utils {

template <typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(std::size_t capacity = 100) : m_capacity(capacity) {}

    void pushBack(const T& value) {
        if (m_capacity == 0) return;
        if (m_items.size() == m_capacity) m_items.pop_front();
        m_items.push_back(value);
    }

    std::size_t size() const { return m_items.size(); }
    std::size_t capacity() const { return m_capacity; }
    bool empty() const { return m_items.empty(); }
    void clear() { m_items.clear(); }

    const std::deque<T>& items() const { return m_items; }

private:
    std::size_t m_capacity = 100;
    std::deque<T> m_items;
};

} // namespace tp::broker::utils
