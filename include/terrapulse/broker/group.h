#pragma once

#include "terrapulse/broker/statistics.h"

#include <string>
#include <unordered_set>

namespace tp::broker {

class Client;

class Group {
public:
    explicit Group(std::string name = {});

    const std::string& name() const { return m_name; }
    std::size_t memberCount() const { return m_members.size(); }

    bool addMember(Client* client);
    bool removeMember(Client* client);
    bool hasMember(const Client* client) const;
    void clearMembers() { m_members.clear(); }

    const std::unordered_set<Client*>& members() const { return m_members; }
    GroupStatistics statistics() const;

private:
    std::string m_name;
    std::unordered_set<Client*> m_members;
    Tx m_messages;
    Tx m_bytes;
    Tx m_payload;

    friend class Queue;
};

} // namespace tp::broker
