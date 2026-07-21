#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QDir>
#include <QUrl>
#include "controllers/AppController.h"
#include "controllers/BusClient.h"
#include "controllers/InventoryModel.h"
#include "controllers/JournalController.h"
#include "bus/Master.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("TerraPulse");
    app.setOrganizationName("TerraPulse");

    // Pure viewer: no processing in the UI. Both the raw stream and the analysis
    // results arrive on the single tpmaster output endpoint (<host>:5562); each
    // subscriber filters by its own prefix ("raw." vs "saf."/"shf.").
    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse operator console");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    QCommandLineOption queueOpt("queue", "Queue to view: production (live) | playback (review/tpolv)", "name", "production");
    parser.addOptions({masterOpt, queueOpt});
    parser.process(app);

    const std::string host  = parser.value(masterOpt).toStdString();
    const auto        queue = tp::master::queueFromName(parser.value(queueOpt).toStdString());
    const std::string out   = tp::master::out(host, queue);

    BusClient         acq(out, "raw.");
    AppController     controller(out);
    InventoryModel    inventory(host, queue);
    JournalController journal(host, queue);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController",     &controller);
    engine.rootContext()->setContextProperty("acq",               &acq);
    engine.rootContext()->setContextProperty("inventory",         &inventory);
    engine.rootContext()->setContextProperty("journalController", &journal);
    engine.rootContext()->setContextProperty("sessionQueue",
                                             QString::fromLatin1(tp::master::queueName(queue)));

    // Offline map tiles (share/maps). TP_SHARE overrides for deployment; else find
    // the source tree's share/maps relative to the executable.
    QString mapsDir = qEnvironmentVariable("TP_SHARE");
    if (!mapsDir.isEmpty()) mapsDir += "/maps";
    QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString& rel : { QStringLiteral("../../share/maps"),
                                QStringLiteral("../../../share/maps"),
                                QStringLiteral("share/maps") }) {
        if (!mapsDir.isEmpty() && QDir(mapsDir).exists()) break;
        const QString cand = appDir.absoluteFilePath(rel);
        if (QDir(cand).exists()) { mapsDir = cand; break; }
    }
    engine.rootContext()->setContextProperty(
        "mapsUrl", mapsDir.isEmpty() ? QString() : QUrl::fromLocalFile(mapsDir).toString());

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("TerraPulse", "Main");
    return app.exec();
}
