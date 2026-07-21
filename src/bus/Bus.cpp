#include "bus/Bus.h"

namespace tp {

zmq::context_t& busContext() {
    static zmq::context_t ctx{1};
    return ctx;
}

// ── Publisher ───────────────────────────────────────────────────────────────

Publisher::Publisher(const std::string& endpoint, bool bind)
    : m_sock(busContext(), zmq::socket_type::pub) {
    if (bind) m_sock.bind(endpoint);
    else      m_sock.connect(endpoint);
}

void Publisher::publish(const BusMessage& msg) {
    m_sock.send(zmq::buffer(msg.topic), zmq::send_flags::sndmore);
    m_sock.send(zmq::buffer(msg.header.constData(), static_cast<size_t>(msg.header.size())),
                zmq::send_flags::sndmore);
    m_sock.send(zmq::buffer(msg.payload.constData(), static_cast<size_t>(msg.payload.size())),
                zmq::send_flags::none);
}

// ── Subscriber ──────────────────────────────────────────────────────────────

Subscriber::Subscriber(const std::string& endpoint)
    : m_sock(busContext(), zmq::socket_type::sub) {
    m_sock.connect(endpoint);
}

void Subscriber::subscribe(const std::string& prefix) {
    m_sock.set(zmq::sockopt::subscribe, prefix);
}

std::optional<BusMessage> Subscriber::receive(int timeoutMs) {
    m_sock.set(zmq::sockopt::rcvtimeo, timeoutMs);

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
