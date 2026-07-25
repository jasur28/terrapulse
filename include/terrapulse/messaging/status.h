#pragma once

#include "terrapulse/core/datetime.h"

#include <QString>

namespace tp::messaging {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct ConnectionStatus {
    ConnectionState state = ConnectionState::Disconnected;
    QString endpoint;
    QString error;
    int messagesSent = 0;
    int messagesReceived = 0;
    tp::core::Time updated = tp::core::Time::now();

    bool connected() const { return state == ConnectionState::Connected; }
};

} // namespace tp::messaging
