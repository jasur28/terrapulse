#pragma once

#include "bus/BusMessage.h"

#include <QByteArray>
#include <QDateTime>

#include <cstdint>
#include <string>

namespace tp::broker {

using SequenceNumber = std::uint64_t;
inline constexpr SequenceNumber kInvalidSequenceNumber = SequenceNumber(-1);

enum class MessageType {
    Unspecified,
    Regular,
    Transient,
    Status
};

struct Message {
    std::string sender;
    std::string target;
    std::string encoding = "identity";
    std::string mimeType = "application/cbor";
    QByteArray header;
    QByteArray payload;
    QDateTime timestampUtc = QDateTime::currentDateTimeUtc();
    MessageType type = MessageType::Regular;
    bool selfDiscard = false;
    bool processed = false;
    SequenceNumber sequenceNumber = 0;

    BusMessage toBusMessage() const {
        return BusMessage{target, header, payload};
    }

    static Message fromBusMessage(const std::string& senderName, const BusMessage& msg) {
        Message out;
        out.sender = senderName;
        out.target = msg.topic;
        out.header = msg.header;
        out.payload = msg.payload;
        return out;
    }
};

} // namespace tp::broker
