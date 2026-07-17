#pragma once
#include "bus/BusMessage.h"
#include <zmq.hpp>
#include <optional>
#include <string>

namespace tp {

// Process-wide shared ZeroMQ context.
zmq::context_t& busContext();

// PUB socket — binds an endpoint, fans out messages to all subscribers.
class Publisher {
public:
    explicit Publisher(const std::string& endpoint);
    void publish(const BusMessage& msg);

private:
    zmq::socket_t m_sock;
};

// SUB socket — connects to a publisher, filters by topic prefix.
class Subscriber {
public:
    explicit Subscriber(const std::string& endpoint);

    // Subscribe to a topic prefix ("" = everything, "saf." = all analysis).
    void subscribe(const std::string& prefix);

    // Block up to timeoutMs (-1 = forever). Returns nullopt on timeout.
    std::optional<BusMessage> receive(int timeoutMs = -1);

private:
    zmq::socket_t m_sock;
};

} // namespace tp
