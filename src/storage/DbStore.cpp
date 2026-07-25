#include "storage/DbStore.h"

#include <QtSql/QSqlError>
#include <cstdio>

namespace tp {

bool DbStore::open(const QString& path, const QString& connName) {
    m_path = path;
    m_db = QSqlDatabase::addDatabase("QSQLITE", connName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        std::fprintf(stderr, "[dbstore] cannot open db '%s': %s\n",
                     path.toUtf8().constData(), m_db.lastError().text().toUtf8().constData());
        return false;
    }
    if (!initSchema()) return false;

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
    return true;
}

bool DbStore::initSchema() {
    QSqlQuery q(m_db);
    // Single-file journal so the .db is always self-contained (easy to inspect).
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
        " max_value REAL, growth_rate REAL, trend INTEGER, confidence REAL,"
        " review INTEGER DEFAULT 0)",   // 0=AUTO 1=CONFIRMED 2=REJECTED

        // ── Grouped anomaly events (tpevent) ────────────────────────────────
        "CREATE TABLE IF NOT EXISTS anomaly_events("
        " event_id INTEGER PRIMARY KEY,"
        " object INTEGER, t_start_ms INTEGER, t_end_ms INTEGER,"
        " status INTEGER, severity INTEGER, anomaly_types TEXT,"
        " sensor_count INTEGER, shf_count INTEGER, review INTEGER DEFAULT 0)",

        // ── Operator journal (audit trail) ──────────────────────────────────
        "CREATE TABLE IF NOT EXISTS journal("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " event_shf_id INTEGER, action TEXT, operator TEXT, note TEXT, t_ms INTEGER)",

        // ── Inventory (data model) ──────────────────────────────────────────
        // network + station_code are the FDSN codes for the stream, distinct
        // from the numeric object_id and the human name. Assigned by the centre;
        // empty station_code falls back to the padded numeric id on read.
        "CREATE TABLE IF NOT EXISTS inv_structures("
        " object_id INTEGER PRIMARY KEY, name TEXT, lat REAL, lon REAL, description TEXT,"
        " network TEXT DEFAULT 'TP', station_code TEXT DEFAULT '')",

        // kind + corner_period describe what the transducer IS, so the channel
        // code (HN? vs EH?) can be computed instead of hard-wired. Defaults make
        // this backward compatible: an old notifier that omits them yields an
        // accelerometer responding to DC — the previous behaviour.
        "CREATE TABLE IF NOT EXISTS inv_sensors("
        " object_id INTEGER, sensor_id INTEGER, model TEXT, location TEXT,"
        " kind TEXT DEFAULT 'accelerometer', corner_period REAL DEFAULT 1e9,"
        " PRIMARY KEY(object_id,sensor_id))",

        "CREATE TABLE IF NOT EXISTS inv_channels("
        " object_id INTEGER, sensor_id INTEGER, component INTEGER,"
        " sample_rate INTEGER, unit TEXT, gain REAL,"
        " PRIMARY KEY(object_id,sensor_id,component))",
    };
    for (const char* s : ddl) {
        if (!q.exec(s)) {
            std::fprintf(stderr, "[dbstore] schema error: %s\n",
                         q.lastError().text().toUtf8().constData());
            return false;
        }
    }

    // Migrate an already-existing DB: CREATE TABLE IF NOT EXISTS won't add the
    // new columns to a table made by an earlier build. ADD COLUMN is a no-op
    // error ("duplicate column") on a fresh DB that already has them — ignore it.
    const char* alters[] = {
        "ALTER TABLE inv_sensors ADD COLUMN kind TEXT DEFAULT 'accelerometer'",
        "ALTER TABLE inv_sensors ADD COLUMN corner_period REAL DEFAULT 1e9",
        "ALTER TABLE inv_structures ADD COLUMN network TEXT DEFAULT 'TP'",
        "ALTER TABLE inv_structures ADD COLUMN station_code TEXT DEFAULT ''",
    };
    for (const char* s : alters) {
        q.exec(s);   // failure means the column already exists; that is fine.
    }
    return true;
}

void DbStore::writeSaf(const QVariantMap& h) {
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
}

void DbStore::writeShf(const QVariantMap& h) {
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
    ++m_events;
}

void DbStore::writeEvent(const QVariantMap& h) {
    // op=remove closes nothing here (events persist); status carries lifecycle.
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO anomaly_events(event_id,object,t_start_ms,t_end_ms,"
              "status,severity,anomaly_types,sensor_count,shf_count,review) "
              "VALUES(?,?,?,?,?,?,?,?,?,COALESCE((SELECT review FROM anomaly_events "
              "WHERE event_id=?),0))");
    q.bindValue(0, h.value("eventId"));
    q.bindValue(1, h.value("objectId"));
    q.bindValue(2, h.value("tStart"));
    q.bindValue(3, h.value("tEnd"));
    q.bindValue(4, h.value("status"));
    q.bindValue(5, h.value("severity"));
    q.bindValue(6, h.value("anomalyTypes"));
    q.bindValue(7, h.value("sensorCount"));
    q.bindValue(8, h.value("shfCount"));
    q.bindValue(9, h.value("eventId"));
    q.exec();
    ++m_anomalyEvents;
}

void DbStore::writeInventory(const QVariantMap& h) {
    const QString kind = h.value("kind").toString();
    const bool    remove = h.value("op").toString() == "remove";
    QSqlQuery q(m_db);

    if (kind == "structure") {
        if (remove) {
            q.prepare("DELETE FROM inv_structures WHERE object_id=?");
            q.bindValue(0, h.value("objectId"));
        } else {
            q.prepare("INSERT OR REPLACE INTO inv_structures"
                      "(object_id,name,lat,lon,description,network,station_code) "
                      "VALUES(?,?,?,?,?,?,?)");
            q.bindValue(0, h.value("objectId"));
            q.bindValue(1, h.value("name"));
            q.bindValue(2, h.value("lat"));
            q.bindValue(3, h.value("lon"));
            q.bindValue(4, h.value("description"));
            q.bindValue(5, h.value("network").toString().isEmpty()
                              ? QVariant("TP") : h.value("network"));
            q.bindValue(6, h.value("stationCode"));   // empty -> derived on read
        }
    } else if (kind == "sensor") {
        if (remove) {
            q.prepare("DELETE FROM inv_sensors WHERE object_id=? AND sensor_id=?");
            q.bindValue(0, h.value("objectId"));
            q.bindValue(1, h.value("sensorId"));
        } else {
            q.prepare("INSERT OR REPLACE INTO inv_sensors"
                      "(object_id,sensor_id,model,location,kind,corner_period) "
                      "VALUES(?,?,?,?,?,?)");
            q.bindValue(0, h.value("objectId"));
            q.bindValue(1, h.value("sensorId"));
            q.bindValue(2, h.value("model"));
            q.bindValue(3, h.value("location"));
            // NB: "kind" here is the notifier's own routing field (== "sensor").
            // The instrument kind travels under "sensorKind" to avoid that clash.
            // Defaults keep an old notifier (no sensorKind/corner) as a
            // DC-coupled accelerometer — the behaviour before this field existed.
            q.bindValue(4, h.value("sensorKind").toString().isEmpty()
                              ? QVariant("accelerometer") : h.value("sensorKind"));
            q.bindValue(5, h.contains("cornerPeriod") ? h.value("cornerPeriod")
                                                       : QVariant(1e9));
        }
    } else if (kind == "channel") {
        if (remove) {
            q.prepare("DELETE FROM inv_channels WHERE object_id=? AND sensor_id=? AND component=?");
            q.bindValue(0, h.value("objectId"));
            q.bindValue(1, h.value("sensorId"));
            q.bindValue(2, h.value("component"));
        } else {
            q.prepare("INSERT OR REPLACE INTO inv_channels(object_id,sensor_id,component,"
                      "sample_rate,unit,gain) VALUES(?,?,?,?,?,?)");
            q.bindValue(0, h.value("objectId"));
            q.bindValue(1, h.value("sensorId"));
            q.bindValue(2, h.value("component"));
            q.bindValue(3, h.value("sampleRate"));
            q.bindValue(4, h.value("unit"));
            q.bindValue(5, h.value("gain"));
        }
    } else {
        return;
    }
    q.exec();
    ++m_invRows;
}

void DbStore::writeJournal(const QVariantMap& h) {
    const qulonglong eventId = h.value("eventId").toULongLong();
    const QString    action  = h.value("action").toString();

    QSqlQuery ins(m_db);
    ins.prepare("INSERT INTO journal(event_shf_id,action,operator,note,t_ms) VALUES(?,?,?,?,?)");
    ins.bindValue(0, eventId);
    ins.bindValue(1, action);
    ins.bindValue(2, h.value("operator"));
    ins.bindValue(3, h.value("note"));
    ins.bindValue(4, h.value("t"));
    ins.exec();
    ++m_journalRows;

    // Confirm/reject/reclassify change the event's review status.
    int review = -1;
    if      (action == "confirm")    review = 1;
    else if (action == "reject")     review = 2;
    else if (action == "reclassify") review = 1;
    if (review >= 0) {
        QSqlQuery upd(m_db);
        upd.prepare("UPDATE events SET review=? WHERE shf_id=?");
        upd.bindValue(0, review);
        upd.bindValue(1, eventId);
        upd.exec();
    }
}

QVariantList DbStore::snapshot() {
    QVariantList out;
    QSqlQuery q(m_db);

    q.exec("SELECT object_id,name,lat,lon,description,network,station_code FROM inv_structures");
    while (q.next()) {
        out.append(QVariantMap{ {"type","notifier"}, {"op","add"}, {"kind","structure"},
            {"objectId",q.value(0)}, {"name",q.value(1)}, {"lat",q.value(2)},
            {"lon",q.value(3)}, {"description",q.value(4)},
            {"network",q.value(5)}, {"stationCode",q.value(6)} });
    }
    q.exec("SELECT object_id,sensor_id,model,location,kind,corner_period FROM inv_sensors");
    while (q.next()) {
        out.append(QVariantMap{ {"type","notifier"}, {"op","add"}, {"kind","sensor"},
            {"objectId",q.value(0)}, {"sensorId",q.value(1)},
            {"model",q.value(2)}, {"location",q.value(3)},
            {"sensorKind",q.value(4)}, {"cornerPeriod",q.value(5)} });
    }
    q.exec("SELECT object_id,sensor_id,component,sample_rate,unit,gain FROM inv_channels");
    while (q.next()) {
        out.append(QVariantMap{ {"type","notifier"}, {"op","add"}, {"kind","channel"},
            {"objectId",q.value(0)}, {"sensorId",q.value(1)}, {"component",q.value(2)},
            {"sampleRate",q.value(3)}, {"unit",q.value(4)}, {"gain",q.value(5)} });
    }
    // Latest SAF per (object,sensor,axis).
    q.exec("SELECT object,sensor,axis,t_ms,rms,dom_freq,health,anomaly_type,warning "
           "FROM saf_index s WHERE t_ms=(SELECT MAX(t_ms) FROM saf_index s2 "
           "WHERE s2.object=s.object AND s2.sensor=s.sensor AND s2.axis=s.axis)");
    while (q.next()) {
        out.append(QVariantMap{ {"type","saf"},
            {"objectId",q.value(0)}, {"sensorId",q.value(1)}, {"component",q.value(2)},
            {"timestamp",q.value(3)}, {"rms",q.value(4)}, {"dominantFrequency",q.value(5)},
            {"healthIndex",q.value(6)}, {"anomalyType",q.value(7)}, {"warningLevel",q.value(8)} });
    }
    return out;
}

} // namespace tp
