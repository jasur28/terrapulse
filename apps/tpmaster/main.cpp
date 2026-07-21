// tpmaster — TerraPulse central message broker.
//
// The single meeting point of the whole system (like SeisComp's scmaster).
// Modules no longer bind their own ports and wire to each other; instead every
// module knows just ONE address — tpmaster — and either publishes to it or
// subscribes from it. Messages are routed by group (topic prefix, see Groups.h).
//
// Transport = a ZeroMQ XSUB/XPUB proxy per QUEUE:
//   * frontend XSUB  (bind)  — publishers PUB here (inbound)
//   * backend  XPUB  (bind)  — subscribers SUB here (outbound)
// Subscriptions flow backend->frontend so the broker only forwards groups that
// somebody is actually listening to.
//
// Queues (like scmaster):
//   production  — the live pipeline; persisted to the main DB (--db).
//   playback    — replay of archived/recorded data (--playback); kept OFF the
//                 production DB (own --playback-db, or no persistence at all) so
//                 re-analysis never pollutes the live archive.
//
//   Usage:  tpmaster [--db <path>] [--playback [--playback-db <path>]] [--quiet]

#include "bus/Groups.h"
#include "bus/BusMessage.h"
#include "bus/Master.h"
#include "storage/DbStore.h"

#include <zmq.hpp>

#include <QCoreApplication>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

std::atomic<bool> g_running{true};
void onSignal(int) { g_running = false; }

// Move one full multipart message from src to dst. Reports the first frame
// (subscription frame on the control path).
bool forwardAll(zmq::socket_t& src, zmq::socket_t& dst, std::string& firstFrame) {
    bool first = true;
    for (;;) {
        zmq::message_t part;
        auto r = src.recv(part, zmq::recv_flags::none);
        if (!r) return false;
        if (first) {
            firstFrame.assign(static_cast<const char*>(part.data()), part.size());
            first = false;
        }
        const bool more = part.more();
        dst.send(part, more ? zmq::send_flags::sndmore : zmq::send_flags::none);
        if (!more) break;
    }
    return true;
}

// Data path: buffer the whole [topic][header][payload] message, optionally
// persist FEATURE/ANOMALY to the DB (write-BEFORE-notify), then forward. Reports
// the topic in `topic`.
bool forwardData(zmq::socket_t& src, zmq::socket_t& dst, std::string& topic, tp::DbStore* store) {
    std::vector<zmq::message_t> frames;
    for (;;) {
        zmq::message_t part;
        auto r = src.recv(part, zmq::recv_flags::none);
        if (!r) return false;
        const bool more = part.more();
        frames.push_back(std::move(part));
        if (!more) break;
    }
    topic.assign(static_cast<const char*>(frames[0].data()), frames[0].size());

    // dbstore: persist before distributing so any client that receives the
    // notification can already read the stored record.
    if (store && frames.size() >= 2) {
        const bool isSaf  = topic.rfind("saf.",  0) == 0;
        const bool isShf  = topic.rfind("shf.",  0) == 0;
        const bool isInv  = topic.rfind("inv.",  0) == 0;
        const bool isJrnl = topic.rfind("jrnl.", 0) == 0;
        if (isSaf || isShf || isInv || isJrnl) {
            QByteArray hb(static_cast<const char*>(frames[1].data()),
                          static_cast<qsizetype>(frames[1].size()));
            const QVariantMap h = tp::BusMessage::decodeHeader(hb);
            if (isSaf)       store->writeSaf(h);
            else if (isShf)  store->writeShf(h);
            else if (isInv)  store->writeInventory(h);
            else             store->writeJournal(h);
        }
    }

    for (std::size_t i = 0; i < frames.size(); ++i)
        dst.send(frames[i], i + 1 < frames.size() ? zmq::send_flags::sndmore
                                                  : zmq::send_flags::none);
    return true;
}

std::string argValue(int argc, char** argv, const char* key, const std::string& def) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], key) == 0) return argv[i + 1];
    return def;
}
bool argFlag(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], key) == 0) return true;
    return false;
}

// One queue = an XSUB/XPUB pair + optional dbstore + throughput stats.
struct Queue {
    std::string   name;
    zmq::socket_t frontend;   // XSUB — publishers in
    zmq::socket_t backend;    // XPUB — subscribers out
    tp::DbStore*  store = nullptr;

    std::array<std::uint64_t, tp::groups::kAll.size()> perGroup{};
    std::uint64_t other = 0, total = 0, lastTotal = 0;

    Queue(zmq::context_t& ctx, std::string n, tp::DbStore* s)
        : name(std::move(n)),
          frontend(ctx, zmq::socket_type::xsub),
          backend(ctx, zmq::socket_type::xpub),
          store(s) {
        backend.set(zmq::sockopt::xpub_verbose, 1);
    }
};

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);   // needed for the QtSql driver (dbstore)

    const std::string dbPath   = argValue(argc, argv, "--db", "");
    const bool playback        = argFlag(argc, argv, "--playback");
    const std::string pbDbPath = argValue(argc, argv, "--playback-db", "");
    const bool quiet           = argFlag(argc, argv, "--quiet");

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    // Optional embedded dbstores (write-before-notify). Only the master writes.
    tp::DbStore prodStore, pbStore;
    tp::DbStore* prodDb = nullptr;
    tp::DbStore* pbDb   = nullptr;
    if (!dbPath.empty()) {
        if (!prodStore.open(QString::fromStdString(dbPath), "tp_prod")) return 1;
        prodDb = &prodStore;
    }
    if (playback && !pbDbPath.empty()) {
        if (!pbStore.open(QString::fromStdString(pbDbPath), "tp_pb")) return 1;
        pbDb = &pbStore;
    }

    zmq::context_t ctx{1};
    std::vector<std::unique_ptr<Queue>> queues;

    auto addQueue = [&](const char* name, tp::master::Queue q, tp::DbStore* db) {
        auto qu = std::make_unique<Queue>(ctx, name, db);
        try {
            qu->frontend.bind("tcp://*:" + std::to_string(tp::master::inPort(q)));
            qu->backend.bind ("tcp://*:" + std::to_string(tp::master::outPort(q)));
        } catch (const zmq::error_t& e) {
            std::fprintf(stderr, "[tpmaster] bind failed on queue '%s': %s\n", name, e.what());
            std::exit(1);
        }
        // With dbstore on, the master itself subscribes to the persisted groups
        // so publishers always deliver them — persistence must not depend on some
        // external client happening to be subscribed.
        if (db) {
            for (const char* p : {"saf.", "shf.", "inv.", "jrnl."}) {
                std::string f;
                f.push_back('\x01');   // ZMQ subscribe frame
                f += p;
                qu->frontend.send(zmq::buffer(f), zmq::send_flags::none);
            }
        }
        queues.push_back(std::move(qu));
    };

    addQueue("production", tp::master::Queue::Production, prodDb);
    if (playback) addQueue("playback", tp::master::Queue::Playback, pbDb);

    // Control socket (REQ/REP): serves data-model snapshots to thin clients.
    zmq::socket_t ctrl{ctx, zmq::socket_type::rep};
    try {
        ctrl.bind("tcp://*:" + std::to_string(tp::master::kCtrlPort));
    } catch (const zmq::error_t& e) {
        std::fprintf(stderr, "[tpmaster] control bind failed: %s\n", e.what());
        return 1;
    }

    std::printf("[tpmaster] up\n");
    for (const auto& q : queues)
        std::printf("[tpmaster] queue '%s'  in(XSUB)=tcp://*:%d  out(XPUB)=tcp://*:%d  dbstore=%s\n",
                    q->name.c_str(),
                    tp::master::inPort(q->name == "playback" ? tp::master::Queue::Playback
                                                             : tp::master::Queue::Production),
                    tp::master::outPort(q->name == "playback" ? tp::master::Queue::Playback
                                                              : tp::master::Queue::Production),
                    q->store ? (q->store->path().toUtf8().constData()) : "off");
    std::printf("[tpmaster] message groups:\n");
    for (const auto& g : tp::groups::kAll)
        std::printf("             %-10.*s %-5.*s %.*s\n",
                    int(g.name.size()),   g.name.data(),
                    int(g.prefix.size()), g.prefix.data(),
                    int(g.desc.size()),   g.desc.data());
    std::printf("[tpmaster] Ctrl+C to stop.\n");
    std::fflush(stdout);

    // Two poll items (data, control) per queue, plus one for the snapshot socket.
    std::vector<zmq::pollitem_t> items;
    for (auto& q : queues) {
        items.push_back({ q->frontend.handle(), 0, ZMQ_POLLIN, 0 });
        items.push_back({ q->backend.handle(),  0, ZMQ_POLLIN, 0 });
    }
    items.push_back({ ctrl.handle(), 0, ZMQ_POLLIN, 0 });
    const std::size_t ctrlIdx = items.size() - 1;

    auto lastStats    = std::chrono::steady_clock::now();
    auto lastReinject = lastStats;
    std::string frame;

    // Re-announce a queue's dbstore subscriptions to its publishers. Publishers
    // that connect (or reconnect) after the initial one-shot injection would
    // otherwise never learn the master wants saf./shf./inv. — so we repeat.
    auto reinject = [](Queue& q) {
        if (!q.store) return;
        for (const char* p : {"saf.", "shf.", "inv.", "jrnl."}) {
            std::string f; f.push_back('\x01'); f += p;
            q.frontend.send(zmq::buffer(f), zmq::send_flags::none);
        }
    };

    while (g_running) {
        try {
            zmq::poll(items.data(), items.size(), std::chrono::milliseconds{500});
        } catch (const zmq::error_t&) {
            continue; // interrupted (e.g. Ctrl+C) — re-check g_running
        }

        for (std::size_t qi = 0; qi < queues.size(); ++qi) {
            Queue& q = *queues[qi];
            zmq::pollitem_t& dataIt = items[2 * qi];
            zmq::pollitem_t& ctrlIt = items[2 * qi + 1];

            // Data path: publishers -> (dbstore) -> subscribers.
            if (dataIt.revents & ZMQ_POLLIN) {
                if (forwardData(q.frontend, q.backend, frame, q.store)) {
                    const int gi = tp::groups::indexForTopic(frame);
                    if (gi < 0) ++q.other; else ++q.perGroup[std::size_t(gi)];
                    ++q.total;
                }
            }
            // Control path: subscriptions/unsubscriptions -> publishers.
            if (ctrlIt.revents & ZMQ_POLLIN) {
                if (forwardAll(q.backend, q.frontend, frame) && !frame.empty() && !quiet) {
                    const bool sub = frame[0] == 1;
                    const std::string prefix = frame.substr(1);
                    std::printf("[tpmaster/%s] %s SUB '%s'  (group %.*s)\n",
                                q.name.c_str(), sub ? "+" : "-", prefix.c_str(),
                                int(tp::groups::groupForTopic(prefix).size()),
                                tp::groups::groupForTopic(prefix).data());
                    std::fflush(stdout);
                }
            }
        }

        // Control: reply to a snapshot request. The request names the queue
        // ("SNAP production" | "SNAP playback"); we serve that queue's dbstore.
        if (items[ctrlIdx].revents & ZMQ_POLLIN) {
            std::string reqStr;
            bool first = true;
            for (zmq::message_t rm; ctrl.recv(rm, zmq::recv_flags::none); ) {
                if (first) { reqStr.assign(static_cast<const char*>(rm.data()), rm.size()); first = false; }
                if (!rm.more()) break;
            }
            tp::DbStore* which = prodDb;
            if (reqStr.find("playback") != std::string::npos && pbDb) which = pbDb;
            const QVariantList snap = which ? which->snapshot() : QVariantList{};
            std::printf("[tpmaster] snapshot request '%s' -> %d objects\n",
                        reqStr.c_str(), int(snap.size()));
            std::fflush(stdout);
            if (snap.isEmpty()) {
                ctrl.send(zmq::buffer(std::string{}), zmq::send_flags::none);
            } else {
                for (int i = 0; i < snap.size(); ++i) {
                    const QByteArray hb = tp::BusMessage::encodeHeader(snap[i].toMap());
                    ctrl.send(zmq::buffer(hb.constData(), static_cast<size_t>(hb.size())),
                              i + 1 < snap.size() ? zmq::send_flags::sndmore
                                                  : zmq::send_flags::none);
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();

        // Periodically re-announce dbstore subscriptions (covers late/reconnected publishers).
        if (now - lastReinject >= std::chrono::milliseconds{750}) {
            lastReinject = now;
            for (auto& qp : queues) reinject(*qp);
        }

        // Periodic throughput line per queue.
        if (!quiet && now - lastStats >= std::chrono::seconds{2}) {
            const auto dt = std::chrono::duration<double>(now - lastStats).count();
            for (auto& qp : queues) {
                Queue& q = *qp;
                const double rate = dt > 0 ? double(q.total - q.lastTotal) / dt : 0.0;
                std::printf("[tpmaster/%s] msgs=%llu (%.0f/s) |", q.name.c_str(),
                            static_cast<unsigned long long>(q.total), rate);
                for (std::size_t i = 0; i < tp::groups::kAll.size(); ++i)
                    if (q.perGroup[i])
                        std::printf(" %.*s=%llu", int(tp::groups::kAll[i].name.size()),
                                    tp::groups::kAll[i].name.data(),
                                    static_cast<unsigned long long>(q.perGroup[i]));
                if (q.other) std::printf(" OTHER=%llu", static_cast<unsigned long long>(q.other));
                if (q.store)
                    std::printf(" | db saf=%llu ev=%llu inv=%llu jr=%llu",
                                static_cast<unsigned long long>(q.store->safRows()),
                                static_cast<unsigned long long>(q.store->events()),
                                static_cast<unsigned long long>(q.store->invRows()),
                                static_cast<unsigned long long>(q.store->journalRows()));
                std::printf("\n");
                q.lastTotal = q.total;
            }
            std::fflush(stdout);
            lastStats = now;
        }
    }

    std::printf("\n[tpmaster] shutting down.\n");
    std::fflush(stdout);
    return 0;
}
