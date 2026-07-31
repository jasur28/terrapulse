#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QUrl>
#include "controllers/AppController.h"
#include "controllers/BusClient.h"
#include "controllers/InventoryModel.h"
#include "controllers/JournalController.h"
#include "controllers/LiveWaveformController.h"
#include "controllers/WaveformService.h"
#include "terrapulse/messaging/inventorymap.h"
#include "bus/Master.h"

int main(int argc, char *argv[]) {
    static QFile logFile(QFileInfo(QString::fromLocal8Bit(argv[0])).absoluteDir().absoluteFilePath("terrapulse-gui.log"));
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& msg) {
            QTextStream out(&logFile);
            const char* level = type == QtDebugMsg ? "DEBUG" :
                                type == QtInfoMsg ? "INFO" :
                                type == QtWarningMsg ? "WARN" :
                                type == QtCriticalMsg ? "CRIT" : "FATAL";
            out << QDateTime::currentDateTime().toString(Qt::ISODate) << " "
                << level << " " << msg << "\n";
            out.flush();
        });
    }

    QQuickStyle::setStyle("Basic");

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
    QCommandLineOption viewOpt("view", "GUI view: full | dashboard | tprttv | tpmap/tpmv | tpolv", "name", "full");
    // Live waveforms come off the SeedLink backbone (a RecordStream), not the bus.
    // --slink host:port points at a tpslinkserver's SeedLink port; empty leaves the
    // live trace inactive (results-only console). --inventory resolves FDSN ids.
    QCommandLineOption slinkOpt("slink", "tpslinkserver SeedLink endpoint host:port for live waveforms", "host:port", "");
    QCommandLineOption invOpt("inventory", "Inventory JSON for FDSN station->object mapping", "file", "");
    QCommandLineOption stationsOpt("stations", "Subscribe to only these stations (SeedLink STATION), comma/space separated, e.g. \"BRDG1,TWRA\". Empty = all", "list", "");
    QCommandLineOption selectOpt("select", "SeedLink channel SELECT patterns, comma/space separated, e.g. \"HN?\" or \"??Z\". Empty = all channels", "list", "");
    parser.addOptions({masterOpt, queueOpt, viewOpt, slinkOpt, invOpt, stationsOpt, selectOpt});
    parser.process(app);

    QString requestedView = parser.value(viewOpt).toLower();
    if (requestedView == "full") {
        const QString exeName = QFileInfo(QString::fromLocal8Bit(argv[0])).baseName().toLower();
        if (exeName == "tprttv" || exeName == "tpmap" || exeName == "tpmv" || exeName == "tpolv")
            requestedView = exeName;
    }
    if (requestedView == "tpmv")
        requestedView = "tpmap";

    const std::string host  = parser.value(masterOpt).toStdString();
    const auto        queue = tp::master::queueFromName(parser.value(queueOpt).toStdString());
    const std::string out   = tp::master::out(host, queue);

    BusClient         acq(out, "raw.");
    AppController     controller(out);
    InventoryModel    inventory(host, queue);
    JournalController journal(host, queue);

    // Live-waveform RecordStream client (SeedLink backbone). Parse "host:port";
    // port 0 (no --slink) leaves it inactive. Station map lets FDSN-named streams
    // resolve to numeric objects, same as the processing consumers.
    QString slinkHost = "127.0.0.1";
    quint16 slinkPort = 0;
    if (const QString sl = parser.value(slinkOpt); !sl.isEmpty()) {
        const int c = sl.lastIndexOf(':');
        slinkHost = c > 0 ? sl.left(c) : sl;
        slinkPort = quint16((c > 0 ? sl.mid(c + 1) : QString()).toUInt());
    }
    // Optional subscription subset (SeisComp-like partition): --stations / --select.
    const QStringList stations  = parser.value(stationsOpt).split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    const QStringList selectors = parser.value(selectOpt).split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    LiveWaveformController liveWaveform(slinkHost, slinkPort,
                                        tp::loadStationMap(parser.value(invOpt)),
                                        stations, selectors);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController",     &controller);
    engine.rootContext()->setContextProperty("acq",               &acq);
    engine.rootContext()->setContextProperty("liveWaveform",      &liveWaveform);
    engine.rootContext()->setContextProperty("inventory",         &inventory);
    engine.rootContext()->setContextProperty("journalController", &journal);
    engine.rootContext()->setContextProperty("sessionQueue",
                                             QString::fromLatin1(tp::master::queueName(queue)));
    engine.rootContext()->setContextProperty("appView", requestedView);
    engine.rootContext()->setContextProperty("singleView", requestedView != "full");

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

    // TDS miniSEED archive for waveform review (written by tpacq/tpslink --archive).
    // TP_TDS overrides; else find var/tds relative to the executable or cwd.
    QString tdsDir = qEnvironmentVariable("TP_TDS");
    for (const QString& rel : { QStringLiteral("../../var/tds"),
                                QStringLiteral("../../../var/tds"),
                                QStringLiteral("var/tds") }) {
        if (!tdsDir.isEmpty() && QDir(tdsDir).exists()) break;
        const QString cand = appDir.absoluteFilePath(rel);
        if (QDir(cand).exists()) { tdsDir = cand; break; }
    }
    static WaveformService waveform(tdsDir);
    engine.rootContext()->setContextProperty("waveform", &waveform);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() {
                         qCritical() << "QML object creation failed";
                         QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     &app, [](const QList<QQmlError>& warnings) {
                         for (const auto& warning : warnings)
                             qWarning().noquote() << warning.toString();
                     });

    engine.loadFromModule("TerraPulse", "Main");
    return app.exec();
}
