#pragma once

#include "terrapulse/broker/message.h"

namespace tp::broker {

class Client;
class Queue;

class MessageDispatcher {
public:
    virtual ~MessageDispatcher() = default;

    virtual void sendMessage(Client* sender, Message message) = 0;
    virtual void messageAvailable(Queue* queue) = 0;
};

} // namespace tp::broker
