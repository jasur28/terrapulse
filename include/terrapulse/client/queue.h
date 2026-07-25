#pragma once

#include "terrapulse/core/exceptions.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

namespace tp::client {

struct Queue {
    std::string name = "production";
    std::string subscribeEndpoint;
    std::string publishEndpoint;
};

class QueueClosedException : public tp::core::Exception {
public:
    QueueClosedException() : Exception("queue has been closed") {}
};

template <typename T>
class ThreadedQueue {
public:
    explicit ThreadedQueue(int capacity = 1024) : m_buffer(static_cast<std::size_t>(capacity)) {}

    void resize(int capacity) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.assign(static_cast<std::size_t>(capacity), T{});
        m_begin = m_end = m_buffered = 0;
    }

    bool push(T value) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_notFull.wait(lock, [&] { return m_closed || m_buffered < m_buffer.size(); });
        if (m_closed) return false;
        m_buffer[m_end] = std::move(value);
        m_end = (m_end + 1) % m_buffer.size();
        ++m_buffered;
        m_notEmpty.notify_one();
        return true;
    }

    bool pushUnique(T value) {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (std::size_t i = 0, pos = m_begin; i < m_buffered; ++i, pos = (pos + 1) % m_buffer.size()) {
            if (m_buffer[pos] == value) return true;
        }
        m_notFull.wait(lock, [&] { return m_closed || m_buffered < m_buffer.size(); });
        if (m_closed) return false;
        m_buffer[m_end] = std::move(value);
        m_end = (m_end + 1) % m_buffer.size();
        ++m_buffered;
        m_notEmpty.notify_one();
        return true;
    }

    T pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_notEmpty.wait(lock, [&] { return m_closed || m_buffered > 0; });
        if (m_closed && m_buffered == 0) throw QueueClosedException();
        T value = std::move(m_buffer[m_begin]);
        m_buffer[m_begin] = T{};
        m_begin = (m_begin + 1) % m_buffer.size();
        --m_buffered;
        m_notFull.notify_one();
        return value;
    }

    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    bool isClosed() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_closed;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffered;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::fill(m_buffer.begin(), m_buffer.end(), T{});
        m_begin = m_end = m_buffered = 0;
        m_closed = false;
    }

private:
    std::vector<T> m_buffer;
    std::size_t m_begin = 0;
    std::size_t m_end = 0;
    std::size_t m_buffered = 0;
    bool m_closed = false;
    mutable std::mutex m_mutex;
    std::condition_variable m_notFull;
    std::condition_variable m_notEmpty;
};

} // namespace tp::client
