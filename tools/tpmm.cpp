// tpmm — TerraPulse module monitor.
// Subscribes to the STATUS group (soh.) from the tpmaster output (XPUB) and
// shows a live table of every module: alive/uptime/throughput/last-seen. This
// is the health of the SYSTEM ITSELF (like SeisComp's scm / SOH view).
//
//   Usage:  tpmm [endpoint] [prefix]
//     tpmm tcp://127.0.0.1:5562 soh.

#include "bus/Bus.h"

#include <QVariantMap>
#include <chrono>
#include <cstdio>
#include <map>
#include <string>

using clk = std::chrono::steady_clock;

int main(int argc, char* argv[]) {
    const std::string endpoint = argc > 1 ? argv[1] : "tcp://127.0.0.1:5562";
    const std::string prefix   = argc > 2 ? argv[2] : "soh.";

    tp::Subscriber sub(endpoint);
    sub.subscribe(prefix);

    struct Entry { QVariantMap h; clk::time_point seen; };
    std::map<std::string, Entry> mods;

    std::printf("[tpmm] watching '%s' on %s\n\n", prefix.c_str(), endpoint.c_str());
    std::fflush(stdout);

    auto lastPrint = clk::now();
    for (;;) {
        auto m = sub.receive(300);
        const auto now = clk::now();
        if (m) {
            const QVariantMap h = tp::BusMessage::decodeHeader(m->header);
            std::string mod = h.value("module").toString().toStdString();
            if (mod.empty()) mod = m->topic; // fall back to topic
            mods[mod] = { h, now };
        }

        if (now - lastPrint >= std::chrono::seconds{1}) {
            lastPrint = now;
            std::printf("%-10s %-8s %-7s %-6s %-6s  %s\n",
                        "MODULE", "PID", "UP(s)", "AGE(s)", "STATE", "COUNTERS");
            for (const auto& [name, e] : mods) {
                const int age = int(std::chrono::duration_cast<std::chrono::seconds>(now - e.seen).count());
                const char* state = age <= 6 ? "UP" : "DOWN";

                std::string counters;
                for (auto it = e.h.constBegin(); it != e.h.constEnd(); ++it) {
                    const QString k = it.key();
                    if (k == "v" || k == "type" || k == "module" || k == "pid" || k == "uptime")
                        continue;
                    if (!counters.empty()) counters += ' ';
                    counters += k.toStdString() + '=' + it.value().toString().toStdString();
                }
                std::printf("%-10s %-8lld %-7d %-6d %-6s  %s\n",
                            name.c_str(),
                            static_cast<long long>(e.h.value("pid").toLongLong()),
                            e.h.value("uptime").toInt(),
                            age, state, counters.c_str());
            }
            std::printf("\n");
            std::fflush(stdout);
        }
    }
}
