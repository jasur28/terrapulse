#pragma once

#include "terrapulse/broker/client.h"
#include "terrapulse/broker/group.h"
#include "terrapulse/broker/messageprocessor.h"
#include "terrapulse/broker/messagedispatcher.h"
#include "terrapulse/broker/statistics.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tp::broker {

class Queue {
public:
    enum class Result {
        Success,
        InternalError,
        ClientNameNotUnique,
        ClientNotAccepted,
        GroupNameNotUnique,
        GroupDoesNotExist,
        GroupAlreadySubscribed,
        GroupNotSubscribed,
        MessageNotAccepted,
        NotEnoughClientHeap
    };

    Queue(std::string name, std::uint64_t maxPayloadSize, std::size_t ringSize = 1000);

    const std::string& name() const { return m_name; }
    std::uint64_t maxPayloadSize() const { return m_maxPayloadSize; }
    SequenceNumber sequenceNumber() const { return m_sequenceNumber; }

    bool add(std::unique_ptr<MessageProcessor> processor);
    Result addGroup(const std::string& name);
    std::vector<std::string> groups() const;

    void setMessageDispatcher(MessageDispatcher* dispatcher) { m_dispatcher = dispatcher; }

    Result connect(Client* client, const MessageProcessor::KeyValues& params = {},
                   MessageProcessor::KeyValues* outParams = nullptr);
    Result disconnect(Client* client);
    Result subscribe(Client* client, const std::string& group);
    Result unsubscribe(Client* client, const std::string& group);

    Result push(Client* sender, Message msg, int packetSize = 0);
    Result dispatch(Client* sender, Message msg);
    void flushProcessedMessages();

    const Message* getMessage(SequenceNumber after, const Client* client) const;
    QueueStatistics statistics(bool reset = false);

private:
    bool publish(Client* sender, const Message& msg);
    Group* findGroupForTarget(const std::string& target);
    const Group* findGroupForTarget(const std::string& target) const;

    std::string m_name;
    std::uint64_t m_maxPayloadSize = 0;
    std::size_t m_ringSize = 1000;
    SequenceNumber m_sequenceNumber = 0;
    std::map<std::string, std::unique_ptr<Group>> m_groups;
    std::map<std::string, Client*> m_clients;
    std::vector<std::unique_ptr<MessageProcessor>> m_processors;
    std::deque<Message> m_messages;
    std::deque<std::pair<Client*, Message>> m_processed;
    MessageDispatcher* m_dispatcher = nullptr;
    Tx m_messagesTx;
    Tx m_bytesTx;
    Tx m_payloadTx;
    mutable std::mutex m_mutex;
};

const char* resultName(Queue::Result result);

} // namespace tp::broker
