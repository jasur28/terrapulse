// tpstore — TerraPulse storage daemon.
// Subscribes to SAF/SHF results and persists them to SQLite: a time-series index
// of analysis results, a one-row-per-event table, and per-sensor latest state.
// No GUI. (Raw/SDF binary archival can be added later.)
//
// This is the standalone SQLite writer, an alternative to tpmaster's embedded
// dbstore for topologies that keep storage as its own process.

#include "terrapulse/client/application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <cstdio>

namespace {

bool initSchema(QSqlDatabase& db) {
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

class StoreApplication : public tp::client::Application {
public:
    StoreApplication(tp::client::ApplicationSettings settings, QString dbPath)
        : Application(std::move(settings)), m_dbPath(std::move(dbPath)) {}

    bool init() override {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(m_dbPath);
        if (!m_db.open()) {
            std::fprintf(stderr, "[tpstore] cannot open db '%s': %s\n",
                         m_dbPath.toUtf8().constData(),
                         m_db.lastError().text().toUtf8().constData());
            return false;
        }
        if (!initSchema(m_db)) return false;

        m_insSaf = QSqlQuery(m_db);
        m_insSaf.prepare(
            "INSERT INTO saf_index(station,object,sensor,axis,t_ms,rms,max_amp,dom_freq,"
            "health,anomaly_type,warning,confidence) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
        m_insSensor = QSqlQuery(m_db);
        m_insSensor.prepare(
            "INSERT INTO sensors(station,object,sensor,last_seen_ms,last_health,last_warning) "
            "VALUES(?,?,?,?,?,?) ON CONFLICT(station,object,sensor) DO UPDATE SET "
            "last_seen_ms=excluded.last_seen_ms, last_health=excluded.last_health, "
            "last_warning=excluded.last_warning");
        m_upsEvent = QSqlQuery(m_db);
        m_upsEvent.prepare(
            "INSERT OR REPLACE INTO events(shf_id,station,object,sensor,axis,type,severity,"
            "status,t_start_ms,t_end_ms,duration_s,max_value,growth_rate,trend,confidence) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");

        if (!Application::init()) return false;

        std::printf("[tpstore] saf/shf <- %s   db=%s\n",
                    messagingUrl().toUtf8().constData(), m_dbPath.toUtf8().constData());
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["saf_rows"] = static_cast<qulonglong>(m_safRows);
        c["events"]   = static_cast<qulonglong>(m_eventRows);
        return c;
    }

protected:
    // One SQLite transaction per drained batch — the same batching the poll loop
    // used before, now driven by the platform.
    void beforeBatch() override { m_db.transaction(); }
    void afterBatch()  override { m_db.commit(); }

    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (topic.startsWith("saf.")) {
            m_insSaf.bindValue(0,  h.value("stationId"));
            m_insSaf.bindValue(1,  h.value("objectId"));
            m_insSaf.bindValue(2,  h.value("sensorId"));
            m_insSaf.bindValue(3,  h.value("component"));
            m_insSaf.bindValue(4,  h.value("timestamp"));
            m_insSaf.bindValue(5,  h.value("rms"));
            m_insSaf.bindValue(6,  h.value("maxAmplitude"));
            m_insSaf.bindValue(7,  h.value("dominantFrequency"));
            m_insSaf.bindValue(8,  h.value("healthIndex"));
            m_insSaf.bindValue(9,  h.value("anomalyType"));
            m_insSaf.bindValue(10, h.value("warningLevel"));
            m_insSaf.bindValue(11, h.value("confidenceLevel"));
            m_insSaf.exec();
            ++m_safRows;

            m_insSensor.bindValue(0, h.value("stationId"));
            m_insSensor.bindValue(1, h.value("objectId"));
            m_insSensor.bindValue(2, h.value("sensorId"));
            m_insSensor.bindValue(3, h.value("timestamp"));
            m_insSensor.bindValue(4, h.value("healthIndex"));
            m_insSensor.bindValue(5, h.value("warningLevel"));
            m_insSensor.exec();
        } else if (topic.startsWith("shf.")) {
            m_upsEvent.bindValue(0,  h.value("shfId"));
            m_upsEvent.bindValue(1,  h.value("stationId"));
            m_upsEvent.bindValue(2,  h.value("objectId"));
            m_upsEvent.bindValue(3,  h.value("sensorId"));
            m_upsEvent.bindValue(4,  h.value("component"));
            m_upsEvent.bindValue(5,  h.value("anomalyType"));
            m_upsEvent.bindValue(6,  h.value("severityLevel"));
            m_upsEvent.bindValue(7,  h.value("anomalyStatus"));
            m_upsEvent.bindValue(8,  h.value("anomalyStartTime"));
            m_upsEvent.bindValue(9,  h.value("anomalyEndTime"));
            m_upsEvent.bindValue(10, h.value("anomalyDuration"));
            m_upsEvent.bindValue(11, h.value("maxValue"));
            m_upsEvent.bindValue(12, h.value("growthRate"));
            m_upsEvent.bindValue(13, h.value("trend"));
            m_upsEvent.bindValue(14, h.value("confidenceLevel"));
            m_upsEvent.exec();
            ++m_eventRows;
        }
    }

    void handleSOH() override {
        std::printf("[tpstore] saf_rows=%llu events=%llu  db=%s\n",
                    static_cast<unsigned long long>(m_safRows),
                    static_cast<unsigned long long>(m_eventRows),
                    m_dbPath.toUtf8().constData());
        std::fflush(stdout);
    }

private:
    QString m_dbPath;
    QSqlDatabase m_db;
    QSqlQuery m_insSaf, m_insSensor, m_upsEvent;
    quint64 m_safRows = 0, m_eventRows = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpstore");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse storage daemon");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host", "127.0.0.1");
    QCommandLineOption dbOpt({"d", "db"}, "SQLite database path", "path", "terrapulse.db");
    parser.addOptions({masterOpt, dbOpt});
    parser.process(app);

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpstore";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = "production";
    settings.subscriptions = {"saf.", "shf."};
    settings.sohIntervalSeconds = 2;

    StoreApplication store(std::move(settings), parser.value(dbOpt));
    return store.exec();
}
