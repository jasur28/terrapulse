#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace tp::broker::utils {

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity = 1024) : m_capacity(capacity) {}

    bool push(T value) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_notFull.wait(lock, [&] { return m_closed || m_items.size() < m_capacity; });
        if (m_closed) return false;
        m_items.push_back(std::move(value));
        m_notEmpty.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_notEmpty.wait(lock, [&] { return m_closed || !m_items.empty(); });
        if (m_items.empty()) return std::nullopt;
        T value = std::move(m_items.front());
        m_items.pop_front();
        m_notFull.notify_one();
        return value;
    }

    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_items.size();
    }

private:
    std::size_t m_capacity = 1024;
    bool m_closed = false;
    std::deque<T> m_items;
    mutable std::mutex m_mutex;
    std::condition_variable m_notFull;
    std::condition_variable m_notEmpty;
};

} // namespace tp::broker::utils
