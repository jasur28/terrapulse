#pragma once

#include <atomic>

namespace tp::core {

class Interruptible {
public:
    void interrupt() { m_interrupted = true; }
    bool interrupted() const { return m_interrupted.load(); }

private:
    std::atomic<bool> m_interrupted{false};
};

} // namespace tp::core
