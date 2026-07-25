#include "terrapulse/broker/queue.h"

#include <QCoreApplication>

#include <cstddef>
#include <iostream>
#include <vector>

class TestClient : public tp::broker::Client {
public:
    using tp::broker::Client::Client;

    std::size_t publish(tp::broker::Client*, const tp::broker::Message& msg) override {
        received.push_back(msg);
        return static_cast<std::size_t>(msg.payload.size() + msg.header.size());
    }

    void enter(const tp::broker::Group&, const tp::broker::Client&, const tp::broker::Message&) override {}
    void leave(const tp::broker::Group&, const tp::broker::Client&, const tp::broker::Message&) override {}
    void disconnected(const tp::broker::Client&, const tp::broker::Message&) override {}
    void ack(tp::broker::SequenceNumber) override {}
    void dispose() override {}

    std::vector<tp::broker::Message> received;
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    tp::broker::Queue queue("production", 1024, 8);
    auto result = queue.addGroup("raw.");
    if (result != tp::broker::Queue::Result::Success) {
        std::cerr << "add raw failed: " << tp::broker::resultName(result) << "\n";
        return 1;
    }
    result = queue.addGroup("qc.");
    if (result != tp::broker::Queue::Result::Success) {
        std::cerr << "add qc failed: " << tp::broker::resultName(result) << "\n";
        return 2;
    }

    TestClient acq("tpacq");
    TestClient rttv("tprttv");
    rttv.setDiscardSelf(true);

    result = queue.connect(&acq);
    if (result != tp::broker::Queue::Result::Success) {
        std::cerr << "connect acq failed: " << tp::broker::resultName(result) << "\n";
        return 3;
    }
    result = queue.connect(&rttv);
    if (result != tp::broker::Queue::Result::Success) {
        std::cerr << "connect rttv failed: " << tp::broker::resultName(result) << "\n";
        return 4;
    }
    result = queue.subscribe(&rttv, "raw.");
    if (result != tp::broker::Queue::Result::Success) {
        std::cerr << "subscribe failed: " << tp::broker::resultName(result) << "\n";
        return 5;
    }

    tp::broker::Message msg;
    msg.sender = acq.name();
    msg.target = "raw.1.1.x";
    msg.payload = QByteArray("abc");
    result = queue.push(&acq, msg, 3);
    if (result != tp::broker::Queue::Result::Success) {
        std::cerr << "push failed: " << tp::broker::resultName(result) << "\n";
        return 6;
    }
    if (rttv.received.size() != 1) {
        std::cerr << "expected 1 message, got " << rttv.received.size() << "\n";
        return 7;
    }

    const tp::broker::Message* replay = queue.getMessage(0, &rttv);
    if (!replay || replay->sequenceNumber != 1) {
        std::cerr << "replay failed\n";
        return 8;
    }

    const auto stats = queue.statistics();
    if (stats.clients != 2 || stats.messages.sent != 1 || stats.groups.empty()) {
        std::cerr << "stats failed clients=" << stats.clients
                  << " sent=" << stats.messages.sent
                  << " groups=" << stats.groups.size() << "\n";
        return 9;
    }

    std::cout << "broker smoke ok\n";
    return 0;
}
