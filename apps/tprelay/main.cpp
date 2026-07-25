// tprelay — forwards messages from one tpmaster to another (SeisComp scimport
// analog). This is what makes the system scale beyond one building: a site master
// keeps full-rate local data, while a regional/national master receives only the
// summary groups it needs. Selecting groups is the whole point — you would not
// ship raw 200 Hz waveforms up the chain.
//
// Messages are relayed byte-for-byte: the header is never re-encoded, so what the
// upstream master stores is exactly what the site master published.
//
//   tprelay --from site-host --to regional-host [--groups evt.,shf.,soh.]
//           [--from-queue production] [--to-queue production]

#include "terrapulse/client/application.h"
#include "terrapulse/messaging/endpoints.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <cstdio>
#include <memory>

namespace {

class RelayApplication : public tp::client::Application {
public:
    RelayApplication(tp::client::ApplicationSettings settings,
                     QString toHost, tp::messaging::Queue toQueue, QString groups)
        : Application(std::move(settings)), m_toHost(std::move(toHost)),
          m_toQueue(toQueue), m_groups(std::move(groups)) {}

    bool init() override {
        if (!Application::init()) return false;
        // The base class connected us to the SOURCE master; the destination is a
        // second, independent connection owned by this module.
        m_out = std::make_unique<tp::messaging::Publisher>(
            tp::messaging::in(m_toHost.toStdString(), m_toQueue), /*bind=*/false);

        std::printf("[tprelay] %s -> %s (%s)   groups: %s\n",
                    messagingUrl().toUtf8().constData(),
                    tp::messaging::in(m_toHost.toStdString(), m_toQueue).c_str(),
                    tp::messaging::queueName(m_toQueue),
                    m_groups.toUtf8().constData());
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["forwarded"] = static_cast<qulonglong>(m_forwarded);
        c["to"]        = m_toHost;
        return c;
    }

protected:
    bool handleRawMessage(const tp::messaging::Message& message) override {
        if (m_out) {
            m_out->publish(message);          // topic/header/payload unchanged
            ++m_forwarded;
        }
        return true;                          // consumed: no decode needed
    }

    void handleSOH() override {
        std::printf("[tprelay] forwarded=%llu\n",
                    static_cast<unsigned long long>(m_forwarded));
        std::fflush(stdout);
    }

private:
    QString m_toHost;
    tp::messaging::Queue m_toQueue;
    QString m_groups;
    std::unique_ptr<tp::messaging::Publisher> m_out;
    quint64 m_forwarded = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tprelay");

    tp::Config cfg;
    cfg.load("tprelay");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse master-to-master relay");
    parser.addHelpOption();
    QCommandLineOption fromOpt("from", "Source tpmaster host", "host",
                               cfg.str("relay.from", "127.0.0.1"));
    QCommandLineOption toOpt("to", "Destination tpmaster host", "host",
                             cfg.str("relay.to", ""));
    QCommandLineOption groupsOpt("groups", "Comma-separated topic prefixes to forward", "list",
                                 cfg.str("relay.groups", "evt.,shf.,inv.,soh."));
    QCommandLineOption fromQOpt("from-queue", "Source queue", "name",
                                cfg.str("relay.fromQueue", "production"));
    QCommandLineOption toQOpt("to-queue", "Destination queue", "name",
                              cfg.str("relay.toQueue", "production"));
    parser.addOptions({fromOpt, toOpt, groupsOpt, fromQOpt, toQOpt});
    parser.process(app);

    if (parser.value(toOpt).isEmpty()) {
        std::fprintf(stderr, "tprelay: --to <host> is required (destination tpmaster)\n");
        return 2;
    }

    const QString groups = parser.value(groupsOpt);
    tp::client::ApplicationSettings settings;
    settings.moduleName = "tprelay";
    settings.masterHost = parser.value(fromOpt);
    settings.queue      = parser.value(fromQOpt);
    for (const QString& g : groups.split(',', Qt::SkipEmptyParts))
        settings.subscriptions << g.trimmed();
    settings.sohIntervalSeconds = 2;

    RelayApplication relay(std::move(settings), parser.value(toOpt),
                           tp::messaging::queueFromName(parser.value(toQOpt).toStdString()),
                           groups);
    return relay.exec();
}
