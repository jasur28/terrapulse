// tpjournal — send an operator action to the JOURNAL group (like scsendjournal).
// The action is persisted by tpmaster's dbstore as an audit row and updates the
// target event's review status.
//
//   Usage:  tpjournal --event <shfId> --action confirm|reject|reclassify|comment
//                     [--operator NAME] [--note "..."] [--master 127.0.0.1]

#include "bus/Bus.h"
#include "bus/Journal.h"
#include "bus/Master.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <cstdio>

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

    const qulonglong eventId = parser.value(eventOpt).toULongLong();
    const std::string endpoint = tp::master::in(parser.value(masterOpt).toStdString());
    tp::Publisher pub(endpoint, /*bind=*/false);

    // Publish after a short delay so the broker's subscription has propagated.
    QTimer::singleShot(1600, [&]() {
        pub.publish(tp::journalMessage(eventId, parser.value(actionOpt),
                                       parser.value(opOpt), parser.value(noteOpt)));
        std::printf("[tpjournal] event=%llu action=%s operator=%s -> %s\n",
                    static_cast<unsigned long long>(eventId),
                    parser.value(actionOpt).toUtf8().constData(),
                    parser.value(opOpt).toUtf8().constData(), endpoint.c_str());
        std::fflush(stdout);
        QTimer::singleShot(400, &app, &QCoreApplication::quit);
    });

    return app.exec();
}
