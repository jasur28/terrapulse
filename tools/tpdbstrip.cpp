// tpdbstrip — database retention (SeisComp scdbstrip). Deletes records older than
// a cut-off so a long-running installation does not grow without bound. Feature
// rows (saf_index) dominate the volume; events/journal are kept longer by default
// because they are the audit trail. Meant to run from a scheduled task.
//
//   tpdbstrip --db terrapulse.db --days 30 [--event-days 365] [--dry-run] [--vacuum]

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <cstdio>

namespace {
qint64 countBefore(QSqlDatabase& db, const QString& table, const QString& col, qint64 cutMs) {
    QSqlQuery q(db);
    q.prepare(QString("SELECT COUNT(*) FROM %1 WHERE %2 < ?").arg(table, col));
    q.bindValue(0, cutMs);
    if (!q.exec() || !q.next()) return 0;
    return q.value(0).toLongLong();
}
qint64 deleteBefore(QSqlDatabase& db, const QString& table, const QString& col, qint64 cutMs) {
    QSqlQuery q(db);
    q.prepare(QString("DELETE FROM %1 WHERE %2 < ?").arg(table, col));
    q.bindValue(0, cutMs);
    if (!q.exec()) {
        std::fprintf(stderr, "  ! %s: %s\n", table.toUtf8().constData(),
                     q.lastError().text().toUtf8().constData());
        return 0;
    }
    return q.numRowsAffected();
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpdbstrip");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse database retention");
    parser.addHelpOption();
    QCommandLineOption dbOpt("db", "SQLite database path", "path", "terrapulse.db");
    QCommandLineOption daysOpt("days", "Keep feature/QC rows for this many days", "n", "30");
    QCommandLineOption evDaysOpt("event-days", "Keep events/journal for this many days", "n", "365");
    QCommandLineOption dryOpt("dry-run", "Report what would be deleted, change nothing");
    QCommandLineOption vacOpt("vacuum", "VACUUM afterwards to reclaim file space");
    parser.addOptions({dbOpt, daysOpt, evDaysOpt, dryOpt, vacOpt});
    parser.process(app);

    const QString path = parser.value(dbOpt);
    const int days     = parser.value(daysOpt).toInt();
    const int evDays   = parser.value(evDaysOpt).toInt();
    const bool dry     = parser.isSet(dryOpt);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tpdbstrip");
    db.setDatabaseName(path);
    if (!db.open()) {
        std::fprintf(stderr, "tpdbstrip: cannot open '%s': %s\n", path.toUtf8().constData(),
                     db.lastError().text().toUtf8().constData());
        return 1;
    }

    const qint64 now    = QDateTime::currentMSecsSinceEpoch();
    const qint64 cut    = now - qint64(days)   * 86400000LL;
    const qint64 cutEv  = now - qint64(evDays) * 86400000LL;

    struct Target { const char* table; const char* col; qint64 cut; };
    const Target targets[] = {
        { "saf_index",      "t_ms",       cut   },
        { "anomaly_events", "t_start_ms", cutEv },
        { "events",         "t_start_ms", cutEv },
        { "journal",        "t_ms",       cutEv },
    };

    std::printf("tpdbstrip: %s\n  features older than %d days (before %s)\n"
                "  events/journal older than %d days\n%s\n",
                path.toUtf8().constData(), days,
                QDateTime::fromMSecsSinceEpoch(cut).toString(Qt::ISODate).toUtf8().constData(),
                evDays, dry ? "  DRY RUN — nothing will be deleted" : "");

    qint64 total = 0;
    for (const Target& t : targets) {
        const qint64 n = dry ? countBefore(db, t.table, t.col, t.cut)
                             : deleteBefore(db, t.table, t.col, t.cut);
        total += n;
        std::printf("  %-16s %s %lld rows\n", t.table,
                    dry ? "would delete" : "deleted", static_cast<long long>(n));
    }

    if (!dry && parser.isSet(vacOpt)) {
        QSqlQuery(db).exec("VACUUM");
        std::printf("  vacuumed\n");
    }
    std::printf("  total: %lld rows\n", static_cast<long long>(total));
    return 0;
}
