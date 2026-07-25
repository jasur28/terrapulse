#include "terrapulse/broker/queue.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace tp::broker {

Client::Client(std::string name) : m_name(std::move(name)) {}

void* Client::memory(int offset) {
    if (offset < 0 || offset >= kMaxLocalHeapSize) throw std::out_of_range("client heap offset");
    return m_heap.data() + offset;
}

const void* Client::memory(int offset) const {
    if (offset < 0 || offset >= kMaxLocalHeapSize) throw std::out_of_range("client heap offset");
    return m_heap.data() + offset;
}

bool Client::setMembershipInformationEnabled(bool enable) {
    m_wantsMembershipInformation = enable;
    return true;
}

bool Client::setDiscardSelf(bool enable) {
    m_discardSelf = enable;
    return true;
}

bool Client::setStatusOnly(bool enable) {
    m_statusOnly = enable;
    return true;
}

void Client::setAcknowledgeWindow(SequenceNumber messages) {
    m_acknowledgeWindow = std::max<SequenceNumber>(1, messages);
}

Group::Group(std::string name) : m_name(std::move(name)) {}

bool Group::addMember(Client* client) {
    return client && m_members.insert(client).second;
}

bool Group::removeMember(Client* client) {
    return m_members.erase(client) > 0;
}

bool Group::hasMember(const Client* client) const {
    return m_members.find(const_cast<Client*>(client)) != m_members.end();
}

GroupStatistics Group::statistics() const {
    GroupStatistics out;
    out.name = m_name;
    out.members = static_cast<std::uint64_t>(m_members.size());
    out.messages = m_messages;
    out.bytes = m_bytes;
    out.payload = m_payload;
    return out;
}

bool Processor::attach(Queue* queue) {
    m_queue = queue;
    return true;
}

Queue::Queue(std::string name, std::uint64_t maxPayloadSize, std::size_t ringSize)
    : m_name(std::move(name)), m_maxPayloadSize(maxPayloadSize), m_ringSize(ringSize) {}

bool Queue::add(std::unique_ptr<MessageProcessor> processor) {
    if (!processor) return false;
    if (!processor->attach(this)) return false;
    m_processors.push_back(std::move(processor));
    return true;
}

Queue::Result Queue::addGroup(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_groups.find(name) != m_groups.end()) return Result::GroupNameNotUnique;
    m_groups.emplace(name, std::make_unique<Group>(name));
    return Result::Success;
}

std::vector<std::string> Queue::groups() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> out;
    out.reserve(m_groups.size());
    for (const auto& item : m_groups) out.push_back(item.first);
    return out;
}

Queue::Result Queue::connect(Client* client, const MessageProcessor::KeyValues& params,
                             MessageProcessor::KeyValues* outParams) {
    if (!client) return Result::InternalError;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (client->name().empty()) {
        client->setName("client-" + std::to_string(m_clients.size() + 1));
    }
    if (m_clients.find(client->name()) != m_clients.end()) return Result::ClientNameNotUnique;

    MessageProcessor::KeyValues localOut;
    for (auto& proc : m_processors) {
        if (!proc->isConnectionProcessingEnabled()) continue;
        if (!proc->acceptConnection(client, params, localOut)) return Result::ClientNotAccepted;
    }
    if (outParams) *outParams = localOut;
    m_clients.emplace(client->name(), client);
    return Result::Success;
}

Queue::Result Queue::disconnect(Client* client) {
    if (!client) return Result::InternalError;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_clients.erase(client->name());
    for (auto& item : m_groups) item.second->removeMember(client);
    for (auto& proc : m_processors) {
        if (proc->isConnectionProcessingEnabled()) proc->dropConnection(client);
    }

    Message msg;
    msg.sender = m_name;
    msg.target = "STATUS_GROUP";
    msg.type = MessageType::Status;
    for (auto& item : m_clients) item.second->disconnected(*client, msg);
    client->dispose();
    return Result::Success;
}

Queue::Result Queue::subscribe(Client* client, const std::string& groupName) {
    if (!client) return Result::InternalError;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_groups.find(groupName);
    if (it == m_groups.end()) return Result::GroupDoesNotExist;
    if (!it->second->addMember(client)) return Result::GroupAlreadySubscribed;

    if (client->wantsMembershipInformation()) {
        Message msg;
        msg.sender = m_name;
        msg.target = groupName;
        msg.type = MessageType::Status;
        client->enter(*it->second, *client, msg);
    }
    return Result::Success;
}

Queue::Result Queue::unsubscribe(Client* client, const std::string& groupName) {
    if (!client) return Result::InternalError;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_groups.find(groupName);
    if (it == m_groups.end()) return Result::GroupDoesNotExist;
    if (!it->second->removeMember(client)) return Result::GroupNotSubscribed;

    Message msg;
    msg.sender = m_name;
    msg.target = groupName;
    msg.type = MessageType::Status;
    client->leave(*it->second, *client, msg);
    return Result::Success;
}

Queue::Result Queue::push(Client* sender, Message msg, int packetSize) {
    if (m_maxPayloadSize > 0 && static_cast<std::uint64_t>(msg.payload.size()) > m_maxPayloadSize)
        return Result::MessageNotAccepted;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        msg.sequenceNumber = ++m_sequenceNumber;
        msg.processed = false;
        ++m_messagesTx.received;
        m_bytesTx.received += static_cast<std::uint64_t>(std::max(0, packetSize));
        m_payloadTx.received += static_cast<std::uint64_t>(msg.payload.size());
    }

    for (auto& proc : m_processors) {
        if (!proc->isMessageProcessingEnabled()) continue;
        if (!proc->process(msg)) return Result::MessageNotAccepted;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        msg.processed = true;
        m_processed.emplace_back(sender, msg);
    }

    if (m_dispatcher) m_dispatcher->messageAvailable(this);
    else flushProcessedMessages();
    return Result::Success;
}

Queue::Result Queue::dispatch(Client* sender, Message msg) {
    if (m_dispatcher) m_dispatcher->sendMessage(sender, std::move(msg));
    else return push(sender, std::move(msg));
    return Result::Success;
}

void Queue::flushProcessedMessages() {
    for (;;) {
        std::pair<Client*, Message> task;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_processed.empty()) break;
            task = std::move(m_processed.front());
            m_processed.pop_front();
        }
        publish(task.first, task.second);
    }
}

bool Queue::publish(Client* sender, const Message& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Group* group = findGroupForTarget(msg.target);
    if (!group) return false;

    for (Client* client : group->members()) {
        if (!client) continue;
        if (client->discardSelf() && sender == client && msg.selfDiscard) continue;
        if (client->statusOnly() && msg.type != MessageType::Status) continue;
        const std::size_t bytes = client->publish(sender, msg);
        group->m_bytes.sent += bytes;
        m_bytesTx.sent += bytes;
        ++group->m_messages.sent;
        ++m_messagesTx.sent;
        group->m_payload.sent += static_cast<std::uint64_t>(msg.payload.size());
        m_payloadTx.sent += static_cast<std::uint64_t>(msg.payload.size());
    }

    m_messages.push_back(msg);
    if (m_messages.size() > m_ringSize) m_messages.pop_front();
    return true;
}

const Message* Queue::getMessage(SequenceNumber after, const Client* client) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const Message& msg : m_messages) {
        if (msg.sequenceNumber <= after) continue;
        const Group* group = findGroupForTarget(msg.target);
        if (!group || !group->hasMember(client)) continue;
        return &msg;
    }
    return nullptr;
}

QueueStatistics Queue::statistics(bool reset) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QueueStatistics out;
    out.name = m_name;
    out.clients = static_cast<std::uint64_t>(m_clients.size());
    out.bufferedMessages = static_cast<std::uint64_t>(m_messages.size());
    out.messages = m_messagesTx;
    out.bytes = m_bytesTx;
    out.payload = m_payloadTx;
    for (const auto& item : m_groups) out.groups.push_back(item.second->statistics());
    if (reset) {
        m_messagesTx = {};
        m_bytesTx = {};
        m_payloadTx = {};
        for (auto& item : m_groups) {
            item.second->m_messages = {};
            item.second->m_bytes = {};
            item.second->m_payload = {};
        }
    }
    return out;
}

Group* Queue::findGroupForTarget(const std::string& target) {
    auto exact = m_groups.find(target);
    if (exact != m_groups.end()) return exact->second.get();
    for (auto& item : m_groups) {
        const std::string& prefix = item.first;
        if (target.rfind(prefix, 0) == 0) return item.second.get();
    }
    return nullptr;
}

const Group* Queue::findGroupForTarget(const std::string& target) const {
    auto exact = m_groups.find(target);
    if (exact != m_groups.end()) return exact->second.get();
    for (const auto& item : m_groups) {
        const std::string& prefix = item.first;
        if (target.rfind(prefix, 0) == 0) return item.second.get();
    }
    return nullptr;
}

const char* resultName(Queue::Result result) {
    switch (result) {
    case Queue::Result::Success: return "Success";
    case Queue::Result::InternalError: return "Internal error";
    case Queue::Result::ClientNameNotUnique: return "Client name is not unique";
    case Queue::Result::ClientNotAccepted: return "Client was not accepted";
    case Queue::Result::GroupNameNotUnique: return "Group name is not unique";
    case Queue::Result::GroupDoesNotExist: return "Group does not exist";
    case Queue::Result::GroupAlreadySubscribed: return "Already subscribed to group";
    case Queue::Result::GroupNotSubscribed: return "Not subscribed to group";
    case Queue::Result::MessageNotAccepted: return "Message not accepted";
    case Queue::Result::NotEnoughClientHeap: return "Not enough client heap";
    }
    return "Unknown";
}

} // namespace tp::broker
