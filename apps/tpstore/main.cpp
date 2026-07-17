// tpstore — TerraPulse storage daemon.
// Subscribes to SAF/SHF results and persists them to SQLite: a time-series index
// of analysis results, a one-row-per-event table, and per-sensor latest state.
// No GUI. (Raw/SDF binary archival can be added later.)

#include "bus/Bus.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariantMap>
#include <cstdio>

static bool initSchema(QSqlDatabase& db) {
    QSqlQuery q(db);
    // Single-file journal so the .db is always self-contained (easy to inspect).
    // DELETE also merges any pre-existing WAL sidecar back into the main file.
    q.exec("PRAGMA journal_mode=DELETE");
    q.exec("PRAGMA synchronous=NORMAL");

    const char* ddl[] = {
        "CREATE TABLE IF NOT EXISTS sensors("
        " station INTEGER, object INTEGER, sensor INTEGER,"
        " last_seen_ms INTEGER, last_health REAL, last_warning INTEGER,"
        " PRIMARY KEY(station,object,sensor))",

        "CREATE TABLE IF NOT EXISTS saf_index("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " station INTEGER, object INTEGER, sensor INTEGER, axis INTEGER,"
        " t_ms INTEGER, rms REAL, max_amp REAL, dom_freq REAL, health REAL,"
        " anomaly_type INTEGER, warning INTEGER, confidence REAL)",

        "CREATE INDEX IF NOT EXISTS idx_saf_sensor_t ON saf_index(sensor, t_ms)",

        "CREATE TABLE IF NOT EXISTS events("
        " shf_id INTEGER PRIMARY KEY,"
        " station INTEGER, object INTEGER, sensor INTEGER, axis INTEGER,"
        " type INTEGER, severity INTEGER, status INTEGER,"
        " t_start_ms INTEGER, t_end_ms INTEGER, duration_s INTEGER,"
        " max_value REAL, growth_rate REAL, trend INTEGER, confidence REAL)",
    };
    for (const char* s : ddl) {
        if (!q.exec(s)) {
            std::fprintf(stderr, "[tpstore] schema error: %s\n",
                         q.lastError().text().toUtf8().constData());
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpstore");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse storage daemon");
    parser.addHelpOption();
    QCommandLineOption subOpt({"s", "sub"}, "Results (SUB) endpoint", "endpoint", "tcp://127.0.0.1:5557");
    QCommandLineOption dbOpt ({"d", "db"},  "SQLite database path",   "path",     "terrapulse.db");
    parser.addOptions({subOpt, dbOpt});
    parser.process(app);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(parser.value(dbOpt));
    if (!db.open()) {
        std::fprintf(stderr, "[tpstore] cannot open db '%s': %s\n",
                     parser.value(dbOpt).toUtf8().constData(),
                     db.lastError().text().toUtf8().constData());
        return 1;
    }
    if (!initSchema(db)) return 1;

    QSqlQuery insSaf(db), insSensor(db), upsEvent(db);
    insSaf.prepare(
        "INSERT INTO saf_index(station,object,sensor,axis,t_ms,rms,max_amp,dom_freq,"
        "health,anomaly_type,warning,confidence) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    insSensor.prepare(
        "INSERT INTO sensors(station,object,sensor,last_seen_ms,last_health,last_warning) "
        "VALUES(?,?,?,?,?,?) ON CONFLICT(station,object,sensor) DO UPDATE SET "
        "last_seen_ms=excluded.last_seen_ms, last_health=excluded.last_health, "
        "last_warning=excluded.last_warning");
    upsEvent.prepare(
        "INSERT OR REPLACE INTO events(shf_id,station,object,sensor,axis,type,severity,"
        "status,t_start_ms,t_end_ms,duration_s,max_value,growth_rate,trend,confidence) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");

    tp::Subscriber sub(parser.value(subOpt).toStdString());
    sub.subscribe("saf.");
    sub.subscribe("shf.");

    quint64 safRows = 0, eventRows = 0;

    QTimer pollTimer;
    QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
        bool any = false;
        db.transaction();
        for (int i = 0; i < 4000; ++i) {
            auto m = sub.receive(0);
            if (!m) break;
            any = true;
            const QVariantMap h = tp::BusMessage::decodeHeader(m->header);

            if (m->topic.rfind("saf.", 0) == 0) {
                insSaf.bindValue(0,  h.value("stationId"));
                insSaf.bindValue(1,  h.value("objectId"));
                insSaf.bindValue(2,  h.value("sensorId"));
                insSaf.bindValue(3,  h.value("component"));
                insSaf.bindValue(4,  h.value("timestamp"));
                insSaf.bindValue(5,  h.value("rms"));
                insSaf.bindValue(6,  h.value("maxAmplitude"));
                insSaf.bindValue(7,  h.value("dominantFrequency"));
                insSaf.bindValue(8,  h.value("healthIndex"));
                insSaf.bindValue(9,  h.value("anomalyType"));
                insSaf.bindValue(10, h.value("warningLevel"));
                insSaf.bindValue(11, h.value("confidenceLevel"));
                insSaf.exec();
                ++safRows;

                insSensor.bindValue(0, h.value("stationId"));
                insSensor.bindValue(1, h.value("objectId"));
                insSensor.bindValue(2, h.value("sensorId"));
                insSensor.bindValue(3, h.value("timestamp"));
                insSensor.bindValue(4, h.value("healthIndex"));
                insSensor.bindValue(5, h.value("warningLevel"));
                insSensor.exec();
            } else if (m->topic.rfind("shf.", 0) == 0) {
                upsEvent.bindValue(0,  h.value("shfId"));
                upsEvent.bindValue(1,  h.value("stationId"));
                upsEvent.bindValue(2,  h.value("objectId"));
                upsEvent.bindValue(3,  h.value("sensorId"));
                upsEvent.bindValue(4,  h.value("component"));
                upsEvent.bindValue(5,  h.value("anomalyType"));
                upsEvent.bindValue(6,  h.value("severityLevel"));
                upsEvent.bindValue(7,  h.value("anomalyStatus"));
                upsEvent.bindValue(8,  h.value("anomalyStartTime"));
                upsEvent.bindValue(9,  h.value("anomalyEndTime"));
                upsEvent.bindValue(10, h.value("anomalyDuration"));
                upsEvent.bindValue(11, h.value("maxValue"));
                upsEvent.bindValue(12, h.value("growthRate"));
                upsEvent.bindValue(13, h.value("trend"));
                upsEvent.bindValue(14, h.value("confidenceLevel"));
                upsEvent.exec();
                ++eventRows;
            }
        }
        db.commit();
        if (!any) { /* nothing this tick */ }
    });
    pollTimer.start(50);

    QTimer stats;
    QObject::connect(&stats, &QTimer::timeout, [&]() {
        std::printf("[tpstore] saf_rows=%llu events=%llu  db=%s\n",
                    static_cast<unsigned long long>(safRows),
                    static_cast<unsigned long long>(eventRows),
                    parser.value(dbOpt).toUtf8().constData());
        std::fflush(stdout);
    });
    stats.start(2000);

    std::printf("[tpstore] saf/shf <- %s   db=%s\n",
                parser.value(subOpt).toUtf8().constData(),
                parser.value(dbOpt).toUtf8().constData());
    std::fflush(stdout);

    return app.exec();
}
