#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tp::broker {

struct Tx {
    std::uint64_t received = 0;
    std::uint64_t sent = 0;

    Tx& operator+=(const Tx& other) {
        received += other.received;
        sent += other.sent;
        return *this;
    }
};

struct GroupStatistics {
    std::string name;
    Tx messages;
    Tx bytes;
    Tx payload;
    std::uint64_t members = 0;
};

struct QueueStatistics {
    std::string name;
    std::vector<GroupStatistics> groups;
    Tx messages;
    Tx bytes;
    Tx payload;
    std::uint64_t clients = 0;
    std::uint64_t bufferedMessages = 0;
};

} // namespace tp::broker
