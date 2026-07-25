#pragma once
#include "bus/BusMessage.h"
#include <zmq.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>

namespace tp {

// Process-wide shared ZeroMQ context.
zmq::context_t& busContext();

// PUB socket — fans out messages to all subscribers.
//   bind=true  (default): binds the endpoint (v2 mesh, direct peers).
//   bind=false          : connects to a broker's XSUB frontend (tpmaster).
//
// Store-and-forward (connect mode only): pass storeForwardCap > 0 to buffer up
// to that many messages while the broker link is down and resend them, oldest
// first, once it reconnects — so an acquisition source loses no data across a
// broker restart. Call pump() periodically (e.g. from a timer) to service the
// reconnect monitor and drain the backlog.
class Publisher {
public:
    explicit Publisher(const std::string& endpoint, bool bind = true,
                       std::size_t storeForwardCap = 0);
    ~Publisher();
    void publish(const BusMessage& msg);

    void        pump();                              // service monitor + drain backlog
    bool        connected() const { return m_connected; }
    std::size_t backlog()   const { return m_backlog.size(); }

private:
    void sendRaw(const BusMessage& msg);

    zmq::socket_t m_sock;

    // store-and-forward state (unused unless storeForwardCap > 0)
    bool                             m_sf        = false;
    bool                             m_connected = true;   // bound sockets are always "up"
    int64_t                          m_connectedAtMs = 0;
    std::size_t                      m_cap       = 0;
    std::deque<BusMessage>           m_backlog;
    std::unique_ptr<zmq::monitor_t>  m_monitor;
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
    int m_timeoutMs = -2;
};

} // namespace tp
