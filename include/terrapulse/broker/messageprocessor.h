#pragma once

#include "terrapulse/broker/message.h"
#include "terrapulse/broker/processor.h"

#include <string>
#include <utility>
#include <vector>

namespace tp::broker {

class Client;

class MessageProcessor : public Processor {
public:
    enum Mode {
        None = 0x00,
        Messages = 0x01,
        Connections = 0x02
    };

    using KeyValuePair = std::pair<std::string, std::string>;
    using KeyValues = std::vector<KeyValuePair>;

    int mode() const { return m_mode; }
    bool isMessageProcessingEnabled() const { return (m_mode & Messages) != 0; }
    bool isConnectionProcessingEnabled() const { return (m_mode & Connections) != 0; }

    virtual bool acceptConnection(Client* client, const KeyValues& inParams, KeyValues& outParams) = 0;
    virtual void dropConnection(Client* client) = 0;
    virtual bool process(Message& msg) = 0;

protected:
    void setMode(int mode) { m_mode = mode; }

private:
    int m_mode = Messages;
};

} // namespace tp::broker
