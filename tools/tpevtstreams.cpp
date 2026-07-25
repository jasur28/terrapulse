// tpevtstreams — for an anomaly event, list the streams and time windows that
// contributed (SeisComp scevtstreams). Feeds the review workflow: pipe the output
// into tpart/tpdump to pull exactly the waveforms an operator needs, instead of
// hunting through the whole archive.
//
//   tpevtstreams --db terrapulse.db --event <id> [--margin 10] [--format list|uri]
//   tpevtstreams --db terrapulse.db --last 5

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <cstdio>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpevtstreams");

    QCommandLineParser parser;
    parser.setApplicationDescription("List streams/time windows for an anomaly event");
    parser.addHelpOption();
    QCommandLineOption dbOpt("db", "SQLite database", "path", "terrapulse.db");
    QCommandLineOption evOpt("event", "Event id", "id");
    QCommandLineOption lastOpt("last", "Instead of one event, list the N most recent", "n");
    QCommandLineOption marginOpt("margin", "Seconds of context around the event", "s", "10");
    QCommandLineOption fmtOpt("format", "list | uri", "f", "list");
    parser.addOptions({dbOpt, evOpt, lastOpt, marginOpt, fmtOpt});
    parser.process(app);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tpevtstreams");
    db.setDatabaseName(parser.value(dbOpt));
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if (!db.open()) {
        std::fprintf(stderr, "tpevtstreams: cannot open '%s': %s\n",
                     parser.value(dbOpt).toUtf8().constData(),
                     db.lastError().text().toUtf8().constData());
        return 1;
    }

    const qint64 marginMs = qint64(parser.value(marginOpt).toDouble() * 1000.0);
    const bool asUri = parser.value(fmtOpt) == "uri";

    QSqlQuery q(db);
    if (parser.isSet(lastOpt)) {
        q.prepare("SELECT event_id, object, t_start_ms, t_end_ms, severity, anomaly_types,"
                  " sensor_count, shf_count FROM anomaly_events ORDER BY t_start_ms DESC LIMIT ?");
        q.bindValue(0, parser.value(lastOpt).toInt());
    } else if (parser.isSet(evOpt)) {
        q.prepare("SELECT event_id, object, t_start_ms, t_end_ms, severity, anomaly_types,"
                  " sensor_count, shf_count FROM anomaly_events WHERE event_id=?");
        q.bindValue(0, parser.value(evOpt).toLongLong());
    } else {
        std::fprintf(stderr, "tpevtstreams: give --event <id> or --last <n>\n");
        return 2;
    }
    if (!q.exec()) {
        std::fprintf(stderr, "tpevtstreams: query failed: %s\n",
                     q.lastError().text().toUtf8().constData());
        return 1;
    }

    int events = 0;
    while (q.next()) {
        ++events;
        const qlonglong evId = q.value(0).toLongLong();
        const int       obj  = q.value(1).toInt();
        const qlonglong t0   = q.value(2).toLongLong() - marginMs;
        qlonglong       t1   = q.value(3).toLongLong();
        if (t1 <= 0) t1 = q.value(2).toLongLong();      // still open: use the start
        t1 += marginMs;

        std::printf("# event %lld  object %d  severity %d  %s  (%d sensor(s), %d detections)\n",
                    evId, obj, q.value(4).toInt(),
                    q.value(5).toString().toUtf8().constData(),
                    q.value(6).toInt(), q.value(7).toInt());

        // Which sensors of that structure actually contributed.
        QSqlQuery s(db);
        s.prepare("SELECT DISTINCT sensor FROM events WHERE object=? AND t_start_ms BETWEEN ? AND ?"
                  " ORDER BY sensor");
        s.bindValue(0, obj);
        s.bindValue(1, q.value(2).toLongLong() - marginMs);
        s.bindValue(2, t1);
        s.exec();
        bool any = false;
        while (s.next()) {
            any = true;
            const int sen = s.value(0).toInt();
            if (asUri)
                std::printf("tds://var/tds?obj=%d&sen=%d  # %lld..%lld\n", obj, sen,
                            static_cast<long long>(t0), static_cast<long long>(t1));
            else
                std::printf("%d;%d;%s;%s\n", obj, sen,
                            QDateTime::fromMSecsSinceEpoch(t0).toString(Qt::ISODate).toUtf8().constData(),
                            QDateTime::fromMSecsSinceEpoch(t1).toString(Qt::ISODate).toUtf8().constData());
        }
        if (!any) std::printf("# (no contributing sensor rows found)\n");
    }
    if (events == 0) std::printf("# no matching events\n");
    return 0;
}
