// tpmon — bus debug monitor. Subscribes to a topic prefix and prints messages.
// Usage: tpmon [endpoint] [prefix]
//   tpmon tcp://127.0.0.1:5556 raw.

#include "bus/Bus.h"
#include <QVariantMap>
#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
    const std::string endpoint = argc > 1 ? argv[1] : "tcp://127.0.0.1:5556";
    const std::string prefix   = argc > 2 ? argv[2] : "";

    tp::Subscriber sub(endpoint);
    sub.subscribe(prefix);
    std::printf("[tpmon] subscribed prefix='%s' on %s\n", prefix.c_str(), endpoint.c_str());
    std::fflush(stdout);

    quint64 n = 0;
    for (;;) {
        auto m = sub.receive(-1);
        if (!m) continue;
        const QVariantMap h = tp::BusMessage::decodeHeader(m->header);
        std::printf("%-16s t=%lld x=%+.4f y=%+.4f z=%+.4f seq=%lld\n",
                    m->topic.c_str(),
                    static_cast<long long>(h.value("t").toLongLong()),
                    h.value("x").toDouble(),
                    h.value("y").toDouble(),
                    h.value("z").toDouble(),
                    static_cast<long long>(h.value("seq").toLongLong()));
        if ((++n % 50) == 0) std::fflush(stdout);
    }
}
