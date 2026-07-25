#pragma once
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace tp {

// SQLite writer for analysis results. Owns the schema and the prepared inserts
// for SAF (feature) and SHF (anomaly) records. Used by tpmaster's embedded
// dbstore (write-before-notify) so the master is the single writer.
class DbStore {
public:
    bool open(const QString& path, const QString& connName = "tp_dbstore");
    void writeSaf(const QVariantMap& h);        // saf_index row + latest sensor state
    void writeShf(const QVariantMap& h);        // events upsert
    void writeEvent(const QVariantMap& h);      // grouped anomaly_events upsert (tpevent)
    void writeInventory(const QVariantMap& h);  // structures/sensors/channels upsert or remove
    void writeJournal(const QVariantMap& h);    // audit row + event review-status update

    // Current data-model snapshot for thin-client backfill: inventory as add-
    // notifier maps + latest SAF per channel as feature maps (type field set).
    QVariantList snapshot();

    quint64 safRows()     const { return m_safRows; }
    quint64 events()      const { return m_events; }
    quint64 anomalyEvents() const { return m_anomalyEvents; }
    quint64 invRows()     const { return m_invRows; }
    quint64 journalRows() const { return m_journalRows; }
    QString path()        const { return m_path; }

private:
    bool initSchema();

    QSqlDatabase m_db;
    QSqlQuery    m_insSaf, m_insSensor, m_upsEvent;
    QString      m_path;
    quint64      m_safRows = 0, m_events = 0, m_anomalyEvents = 0, m_invRows = 0, m_journalRows = 0;
};

} // namespace tp
