// tpdiskmon — disk-space watchdog (SeisComp's diskmon). A monitoring station that
// runs out of disk stops recording silently, which is the worst possible failure
// for an archive. This reports (and optionally alerts) but never deletes —
// retention is tpdbstrip's job, deliberately kept separate.
//
//   tpdiskmon [--path var/tds] [--path var/logs] [--warn-pct 15] [--crit-pct 5]
//             [--once] [--master host]

#include "bus/Bus.h"
#include "bus/Soh.h"
#include "bus/Master.h"
#include "config/Config.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QStorageInfo>
#include <QStringList>
#include <QTimer>
#include <cstdio>
#include <memory>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpdiskmon");

    tp::Config cfg;
    cfg.load("tpdiskmon");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse disk-space watchdog");
    parser.addHelpOption();
    QCommandLineOption pathOpt("path", "Path to watch (repeatable)", "dir");
    QCommandLineOption warnOpt("warn-pct", "Warn below this % free", "n",
                               QString::number(cfg.integer("diskmon.warnPct", 15)));
    QCommandLineOption critOpt("crit-pct", "Critical below this % free", "n",
                               QString::number(cfg.integer("diskmon.critPct", 5)));
    QCommandLineOption everyOpt("every-sec", "Check interval (s)", "s",
                                QString::number(cfg.integer("diskmon.everySec", 300)));
    QCommandLineOption onceOpt("once", "Check once and exit (for cron/Task Scheduler)");
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host (publishes SOH)", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    parser.addOptions({pathOpt, warnOpt, critOpt, everyOpt, onceOpt, masterOpt});
    parser.process(app);

    QStringList paths = parser.values(pathOpt);
    if (paths.isEmpty())
        paths = cfg.str("diskmon.paths", ".").split(',', Qt::SkipEmptyParts);

    const int warnPct = parser.value(warnOpt).toInt();
    const int critPct = parser.value(critOpt).toInt();
    const bool once   = parser.isSet(onceOpt);

    // Only open a bus connection when we will actually keep running.
    std::unique_ptr<tp::Publisher> pub;
    if (!once)
        pub = std::make_unique<tp::Publisher>(
            tp::master::in(parser.value(masterOpt).toStdString(), tp::master::Queue::Production),
            /*bind=*/false);

    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    int worstExit = 0;

    auto check = [&]() {
        QVariantMap counters;
        int worst = 0;
        for (const QString& p : paths) {
            const QString path = p.trimmed();
            if (path.isEmpty()) continue;
            QStorageInfo si{QDir(path).absolutePath()};
            if (!si.isValid() || !si.isReady()) {
                std::printf("[tpdiskmon] %-24s  (not ready)\n", path.toUtf8().constData());
                continue;
            }
            const double freeGb  = double(si.bytesAvailable()) / (1024.0 * 1024.0 * 1024.0);
            const double totalGb = double(si.bytesTotal())     / (1024.0 * 1024.0 * 1024.0);
            const double pct     = totalGb > 0 ? 100.0 * freeGb / totalGb : 0.0;
            const char*  state   = pct < critPct ? "CRITICAL" : pct < warnPct ? "WARNING" : "OK";
            worst = qMax(worst, pct < critPct ? 2 : pct < warnPct ? 1 : 0);

            std::printf("[tpdiskmon] %-24s %-8s %.1f%% free (%.1f of %.1f GB)\n",
                        path.toUtf8().constData(), state, pct, freeGb, totalGb);
            counters[path] = QString::asprintf("%.1f%%", pct);
        }
        std::fflush(stdout);
        worstExit = worst;
        if (pub) {
            counters["state"] = worst == 2 ? "CRITICAL" : worst == 1 ? "WARNING" : "OK";
            pub->publish(tp::sohMessage("tpdiskmon", startMs, counters));
        }
    };

    check();
    if (once) return worstExit;      // non-zero exit lets a scheduler act on it

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, check);
    timer.start(qMax(10, parser.value(everyOpt).toInt()) * 1000);
    return app.exec();
}
