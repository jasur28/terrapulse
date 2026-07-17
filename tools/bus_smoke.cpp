// ZeroMQ de-risking smoke test for the TerraPulse bus.
// Publishes 5 SAF-topic messages and verifies the subscriber receives all 5,
// round-tripping the CBOR header. Prints "SMOKE OK (5/5)" on success.

#include "bus/Bus.h"
#include <QVariantMap>
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    using namespace tp;
    const std::string endpoint = "tcp://127.0.0.1:5599";

    Publisher  pub(endpoint);
    Subscriber sub(endpoint);
    sub.subscribe("saf.");

    // PUB/SUB slow-joiner: give the subscription time to propagate.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    for (int i = 0; i < 5; ++i) {
        QVariantMap h;
        h["v"]      = 1;
        h["type"]   = "saf";
        h["sensor"] = 1;
        h["axis"]   = 2;
        h["seq"]    = i;

        BusMessage msg;
        msg.topic   = "saf.1.1.1.2";
        msg.header  = BusMessage::encodeHeader(h);
        msg.payload = QByteArray("payload-") + QByteArray::number(i);
        pub.publish(msg);
    }

    int received = 0;
    for (int i = 0; i < 5; ++i) {
        auto m = sub.receive(1000);
        if (!m) break;
        const QVariantMap h = BusMessage::decodeHeader(m->header);
        std::printf("RX topic=%s seq=%lld payload=%s\n",
                    m->topic.c_str(),
                    static_cast<long long>(h.value("seq").toLongLong()),
                    m->payload.constData());
        ++received;
    }

    if (received == 5) {
        std::printf("SMOKE OK (%d/5)\n", received);
        return 0;
    }
    std::printf("SMOKE PARTIAL (%d/5)\n", received);
    return 1;
}
