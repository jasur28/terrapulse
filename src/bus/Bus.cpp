#include "bus/Bus.h"

#include <atomic>
#include <chrono>

namespace tp {

zmq::context_t& busContext() {
    static zmq::context_t ctx{1};
    return ctx;
}

namespace {
int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Flips a pair of flags on broker connect/disconnect for store-and-forward.
class ConnMonitor : public zmq::monitor_t {
public:
    bool*    connected      = nullptr;
    int64_t* connectedAtMs  = nullptr;
    void on_event_connected(const zmq_event_t&, const char*) override {
        if (connected) *connected = true;
        if (connectedAtMs) *connectedAtMs = nowMs();
    }
    void on_event_disconnected(const zmq_event_t&, const char*) override {
        if (connected) *connected = false;
    }
};
} // namespace

// ── Publisher ───────────────────────────────────────────────────────────────

Publisher::Publisher(const std::string& endpoint, bool bind, std::size_t storeForwardCap)
    : m_sock(busContext(), zmq::socket_type::pub) {
    m_sock.set(zmq::sockopt::linger, 0);
    m_sock.set(zmq::sockopt::sndhwm, 10000);
    if (bind) {
        m_sock.bind(endpoint);           // a bound socket is always "up"
    } else {
        if (storeForwardCap > 0) {
            m_sf = true;
            m_cap = storeForwardCap;
            m_connected = false;         // buffer until the broker link comes up
            // Install the monitor BEFORE connect so the first CONNECTED is caught.
            static std::atomic<unsigned> tag{0};
            const std::string mon = "inproc://tp-sf-mon-" + std::to_string(tag++);
            auto cm = std::make_unique<ConnMonitor>();
            cm->connected     = &m_connected;
            cm->connectedAtMs = &m_connectedAtMs;
            cm->init(m_sock, mon, ZMQ_EVENT_CONNECTED | ZMQ_EVENT_DISCONNECTED);
            m_monitor = std::move(cm);
        }
        m_sock.connect(endpoint);
    }
}

Publisher::~Publisher() = default;

void Publisher::sendRaw(const BusMessage& msg) {
    m_sock.send(zmq::buffer(msg.topic), zmq::send_flags::sndmore);
    m_sock.send(zmq::buffer(msg.header.constData(), static_cast<size_t>(msg.header.size())),
                zmq::send_flags::sndmore);
    m_sock.send(zmq::buffer(msg.payload.constData(), static_cast<size_t>(msg.payload.size())),
                zmq::send_flags::none);
}

void Publisher::publish(const BusMessage& msg) {
    if (m_sf && !m_connected) {
        m_backlog.push_back(msg);
        while (m_backlog.size() > m_cap) m_backlog.pop_front();   // drop oldest
        return;
    }
    sendRaw(msg);
}

void Publisher::pump() {
    if (!m_sf || !m_monitor) return;
    while (m_monitor->check_event(0)) {}                 // process reconnect events
    // Drain once the link has settled (small guard against PUB/SUB slow-joiner).
    if (m_connected && !m_backlog.empty() && nowMs() - m_connectedAtMs > 300) {
        while (!m_backlog.empty()) {
            sendRaw(m_backlog.front());
            m_backlog.pop_front();
        }
    }
}

// ── Subscriber ──────────────────────────────────────────────────────────────

Subscriber::Subscriber(const std::string& endpoint)
    : m_sock(busContext(), zmq::socket_type::sub) {
    m_sock.set(zmq::sockopt::linger, 0);
    m_sock.set(zmq::sockopt::rcvhwm, 10000);
    m_sock.connect(endpoint);
}

void Subscriber::subscribe(const std::string& prefix) {
    m_sock.set(zmq::sockopt::subscribe, prefix);
}

std::optional<BusMessage> Subscriber::receive(int timeoutMs) {
    if (m_timeoutMs != timeoutMs) {
        m_sock.set(zmq::sockopt::rcvtimeo, timeoutMs);
        m_timeoutMs = timeoutMs;
    }

    zmq::message_t topic;
    auto r = m_sock.recv(topic, zmq::recv_flags::none);
    if (!r) return std::nullopt; // timed out

    zmq::message_t header, payload;
    (void)m_sock.recv(header,  zmq::recv_flags::none);
    (void)m_sock.recv(payload, zmq::recv_flags::none);

    BusMessage msg;
    msg.topic.assign(static_cast<const char*>(topic.data()), topic.size());
    msg.header  = QByteArray(static_cast<const char*>(header.data()),  static_cast<qsizetype>(header.size()));
    msg.payload = QByteArray(static_cast<const char*>(payload.data()), static_cast<qsizetype>(payload.size()));
    return msg;
}

} // namespace tp
