#pragma once

#include "terrapulse/broker/message.h"

#include <QDateTime>

#include <array>
#include <cstddef>
#include <string>

namespace tp::broker {

class Group;

class Client {
public:
    static constexpr int kMaxLocalHeapSize = 128;

    explicit Client(std::string name = {});
    virtual ~Client() = default;

    const std::string& name() const { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    const QDateTime& created() const { return m_created; }

    void* memory(int offset);
    const void* memory(int offset) const;

    bool setMembershipInformationEnabled(bool enable);
    bool wantsMembershipInformation() const { return m_wantsMembershipInformation; }

    bool setDiscardSelf(bool enable);
    bool discardSelf() const { return m_discardSelf; }

    bool setStatusOnly(bool enable);
    bool statusOnly() const { return m_statusOnly; }

    void setAcknowledgeWindow(SequenceNumber messages);
    SequenceNumber acknowledgeWindow() const { return m_acknowledgeWindow; }

    virtual std::string remoteAddress() const { return {}; }
    virtual std::size_t publish(Client* sender, const Message& msg) = 0;
    virtual void enter(const Group& group, const Client& newMember, const Message& msg) = 0;
    virtual void leave(const Group& group, const Client& oldMember, const Message& msg) = 0;
    virtual void disconnected(const Client& disconnectedClient, const Message& msg) = 0;
    virtual void ack(SequenceNumber sequenceNumber) = 0;
    virtual void dispose() = 0;

private:
    std::string m_name;
    QDateTime m_created = QDateTime::currentDateTimeUtc();
    bool m_wantsMembershipInformation = false;
    bool m_discardSelf = false;
    bool m_statusOnly = false;
    SequenceNumber m_acknowledgeWindow = 20;
    std::array<char, kMaxLocalHeapSize> m_heap{};
};

} // namespace tp::broker
