// tpjournal — send an operator action to the JOURNAL group (like scsendjournal).
// The action is persisted by tpmaster's dbstore as an audit row and updates the
// target event's review status.
//
//   Usage:  tpjournal --event <shfId> --action confirm|reject|reclassify|comment
//                     [--operator NAME] [--note "..."] [--master 127.0.0.1]

#include "terrapulse/client/application.h"
#include "terrapulse/messaging/journal.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <cstdio>

namespace {

// A one-shot publisher: no subscriptions, no heartbeat. init() schedules the
// single message (after the slow-joiner settle) and then quits.
class JournalApplication : public tp::client::Application {
public:
    JournalApplication(tp::client::ApplicationSettings settings, qulonglong eventId,
                       QString action, QString op, QString note)
        : Application(std::move(settings)), m_eventId(eventId),
          m_action(std::move(action)), m_op(std::move(op)), m_note(std::move(note)) {}

    bool init() override {
        if (!Application::init()) return false;
        publishThenQuit([this]() {
            publish(tp::messaging::journal(m_eventId, m_action, m_op, m_note));
            std::printf("[tpjournal] event=%llu action=%s operator=%s -> %s\n",
                        static_cast<unsigned long long>(m_eventId),
                        m_action.toUtf8().constData(), m_op.toUtf8().constData(),
                        messagingUrl().toUtf8().constData());
            std::fflush(stdout);
        });
        return true;
    }

private:
    qulonglong m_eventId;
    QString m_action, m_op, m_note;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpjournal");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse operator journal");
    parser.addHelpOption();
    QCommandLineOption eventOpt ("event",    "Target event (SHF) id", "id");
    QCommandLineOption actionOpt("action",   "confirm | reject | reclassify | comment", "act", "confirm");
    QCommandLineOption opOpt     ("operator", "Operator name", "name", "operator");
    QCommandLineOption noteOpt   ("note",     "Free-text note", "text", "");
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    parser.addOptions({eventOpt, actionOpt, opOpt, noteOpt, masterOpt});
    parser.process(app);

    if (!parser.isSet(eventOpt)) {
        std::fprintf(stderr, "tpjournal: --event <shfId> is required\n");
        return 2;
    }

    tp::client::ApplicationSettings settings;
    settings.moduleName = "tpjournal";
    settings.masterHost = parser.value(masterOpt);
    settings.queue      = "production";
    settings.sohIntervalSeconds = 0;        // one-shot: no heartbeat

    JournalApplication journal(std::move(settings),
                               parser.value(eventOpt).toULongLong(),
                               parser.value(actionOpt), parser.value(opOpt),
                               parser.value(noteOpt));
    return journal.exec();
}
