#pragma once

#include "terrapulse/messaging/message.h"

#include <QByteArray>

namespace tp::messaging {

struct Packet {
    QString group;
    QString topic;
    QByteArray header;
    QByteArray payload;

    bool empty() const { return topic.isEmpty() && header.isEmpty() && payload.isEmpty(); }
};

} // namespace tp::messaging
