// tpxmldump — export the data model as XML (SeisComp scxmldump / SCML). Gives a
// portable, human-readable snapshot for archiving, hand-off to a client, or
// import into another installation — independent of our SQLite schema.
//
//   tpxmldump --db terrapulse.db [--inventory] [--events] [--journal] [--all]
//             [--limit 500] [--out dump.xml]

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <cstdio>

namespace {

QString esc(const QString& s) {
    QString o = s;
    o.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;");
    return o;
}

// Emit each row as <tag attr="value" .../> using the query's column names.
void dumpRows(QTextStream& out, QSqlQuery& q, const QString& tag, int indent) {
    const QString pad(indent, ' ');
    while (q.next()) {
        const QSqlRecord r = q.record();
        out << pad << "<" << tag;
        for (int i = 0; i < r.count(); ++i)
            out << " " << r.fieldName(i) << "=\"" << esc(r.value(i).toString()) << "\"";
        out << "/>\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpxmldump");

    QCommandLineParser parser;
    parser.setApplicationDescription("Export the TerraPulse data model as XML");
    parser.addHelpOption();
    QCommandLineOption dbOpt("db", "SQLite database", "path", "terrapulse.db");
    QCommandLineOption invOpt("inventory", "Include inventory");
    QCommandLineOption evOpt("events", "Include anomalies and events");
    QCommandLineOption jrOpt("journal", "Include the operator journal");
    QCommandLineOption allOpt("all", "Include everything");
    QCommandLineOption limOpt("limit", "Max rows per table", "n", "1000");
    QCommandLineOption outOpt("out", "Write to a file instead of stdout", "path");
    parser.addOptions({dbOpt, invOpt, evOpt, jrOpt, allOpt, limOpt, outOpt});
    parser.process(app);

    const bool all = parser.isSet(allOpt);
    const bool wantInv = all || parser.isSet(invOpt);
    const bool wantEv  = all || parser.isSet(evOpt);
    const bool wantJr  = all || parser.isSet(jrOpt);
    if (!wantInv && !wantEv && !wantJr) {
        std::fprintf(stderr, "tpxmldump: choose --inventory / --events / --journal / --all\n");
        return 2;
    }
    const int limit = parser.value(limOpt).toInt();

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tpxmldump");
    db.setDatabaseName(parser.value(dbOpt));
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if (!db.open()) {
        std::fprintf(stderr, "tpxmldump: cannot open '%s': %s\n",
                     parser.value(dbOpt).toUtf8().constData(),
                     db.lastError().text().toUtf8().constData());
        return 1;
    }

    QFile file;
    QTextStream out;
    if (parser.isSet(outOpt)) {
        file.setFileName(parser.value(outOpt));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            std::fprintf(stderr, "tpxmldump: cannot write '%s'\n",
                         parser.value(outOpt).toUtf8().constData());
            return 1;
        }
        out.setDevice(&file);
    } else {
        out.setDevice(nullptr);
        static QFile stdoutFile;
        stdoutFile.open(stdout, QIODevice::WriteOnly | QIODevice::Text);
        out.setDevice(&stdoutFile);
    }

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<terrapulse version=\"1\" generated=\""
        << QDateTime::currentDateTime().toString(Qt::ISODate) << "\">\n";

    QSqlQuery q(db);
    if (wantInv) {
        out << "  <inventory>\n";
        q.exec("SELECT object_id,name,lat,lon,description FROM inv_structures ORDER BY object_id");
        dumpRows(out, q, "structure", 4);
        q.exec("SELECT object_id,sensor_id,model,location FROM inv_sensors ORDER BY object_id,sensor_id");
        dumpRows(out, q, "sensor", 4);
        q.exec("SELECT object_id,sensor_id,component,sample_rate,unit,gain FROM inv_channels"
               " ORDER BY object_id,sensor_id,component");
        dumpRows(out, q, "channel", 4);
        out << "  </inventory>\n";
    }
    if (wantEv) {
        out << "  <events>\n";
        q.exec(QString("SELECT event_id,object,t_start_ms,t_end_ms,status,severity,anomaly_types,"
                       "sensor_count,shf_count,review FROM anomaly_events"
                       " ORDER BY t_start_ms DESC LIMIT %1").arg(limit));
        dumpRows(out, q, "event", 4);
        q.exec(QString("SELECT shf_id,object,sensor,axis,type,severity,status,t_start_ms,t_end_ms,"
                       "duration_s,max_value,confidence,review FROM events"
                       " ORDER BY t_start_ms DESC LIMIT %1").arg(limit));
        dumpRows(out, q, "anomaly", 4);
        out << "  </events>\n";
    }
    if (wantJr) {
        out << "  <journal>\n";
        q.exec(QString("SELECT id,event_shf_id,action,operator,note,t_ms FROM journal"
                       " ORDER BY t_ms DESC LIMIT %1").arg(limit));
        dumpRows(out, q, "entry", 4);
        out << "  </journal>\n";
    }
    out << "</terrapulse>\n";
    out.flush();
    return 0;
}
