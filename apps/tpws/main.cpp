// tpws — web service (SeisComp fdsnws analog). Exposes the data model over HTTP
// as JSON so web and mobile clients can read it without any TerraPulse libraries,
// plus a live Server-Sent Events stream so a browser page updates in real time.
//
// Read-only by design: it opens the database read-only and never publishes to the
// bus, so exposing it cannot affect the monitoring pipeline.
//
//   tpws [--db terrapulse.db] [--port 8080] [--master host] [--bind 0.0.0.0]
//
// Endpoints:
//   GET /api/health                                  service + counts
//   GET /api/structures                              inventory: monitored objects
//   GET /api/sensors                                 sensors + latest health
//   GET /api/events?limit=50                         grouped anomaly events
//   GET /api/anomalies?limit=50                      individual anomalies (SHF)
//   GET /api/features?object=1&sensor=1&limit=200    analysis history (SAF)
//   GET /api/waveform?object=1&sensor=1&start=..&end=..   recorded X/Y/Z from TDS
//   GET /api/stream                                  live updates (text/event-stream)

#include "terrapulse/client/application.h"
#include "mseed/RecordStream.h"
#include "mseed/Mseed.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <cstdio>

namespace {

QByteArray httpResponse(int code, const QByteArray& contentType, const QByteArray& body) {
    const char* reason = code == 200 ? "OK" : code == 404 ? "Not Found" : "Bad Request";
    QByteArray h;
    h += "HTTP/1.1 " + QByteArray::number(code) + " " + reason + "\r\n";
    h += "Content-Type: " + contentType + "\r\n";
    h += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    h += "Access-Control-Allow-Origin: *\r\n";     // usable from a browser page
    h += "Connection: close\r\n\r\n";
    return h + body;
}

QByteArray json(const QJsonDocument& doc) { return doc.toJson(QJsonDocument::Compact); }

// Run a query and return its rows as a JSON array of objects (column-named).
QJsonArray rowsToJson(QSqlQuery& q) {
    QJsonArray arr;
    while (q.next()) {
        const QSqlRecord r = q.record();
        QJsonObject o;
        for (int i = 0; i < r.count(); ++i)
            o[r.fieldName(i)] = QJsonValue::fromVariant(r.value(i));
        arr.append(o);
    }
    return arr;
}

int clampLimit(const QUrlQuery& q, int def, int max) {
    bool ok = false;
    const int v = q.queryItemValue("limit").toInt(&ok);
    return (ok && v > 0) ? qMin(v, max) : def;
}

class WsApplication : public tp::client::Application {
public:
    WsApplication(tp::client::ApplicationSettings settings, QString dbPath,
                  QString tdsRoot, QString bind, quint16 port)
        : Application(std::move(settings)), m_dbPath(std::move(dbPath)),
          m_tdsRoot(std::move(tdsRoot)), m_bind(std::move(bind)), m_port(port) {}

    bool init() override {
        // Read-only DB handle: this service must never modify the archive.
        m_db = QSqlDatabase::addDatabase("QSQLITE", "tpws");
        m_db.setDatabaseName(m_dbPath);
        m_db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!m_db.open()) {
            std::fprintf(stderr, "tpws: cannot open '%s': %s\n",
                         m_dbPath.toUtf8().constData(),
                         m_db.lastError().text().toUtf8().constData());
            return false;
        }

        if (!Application::init()) return false;      // subscribes the stream groups

        QObject::connect(&m_server, &QTcpServer::newConnection, [this]() { accept(); });
        if (!m_server.listen(QHostAddress(m_bind), m_port)) {
            std::fprintf(stderr, "tpws: cannot listen on %s:%u: %s\n",
                         m_bind.toUtf8().constData(), m_port,
                         m_server.errorString().toUtf8().constData());
            return false;
        }
        // A read-only service publishes nothing to the bus (not even a heartbeat),
        // so it prints its own stats rather than emitting SOH.
        QObject::connect(&m_statsTimer, &QTimer::timeout, [this]() {
            std::printf("[tpws] requests=%llu streamed=%llu sseClients=%d\n",
                        static_cast<unsigned long long>(m_requests),
                        static_cast<unsigned long long>(m_streamed), int(m_sseClients.size()));
            std::fflush(stdout);
        });
        m_statsTimer.start(5000);

        std::printf("[tpws] http://%s:%u/api  (db %s, read-only)\n",
                    m_bind.toUtf8().constData(), m_port, m_dbPath.toUtf8().constData());
        std::fflush(stdout);
        return true;
    }

protected:
    // Live stream: every subscribed message is pushed to the open SSE clients.
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (m_sseClients.isEmpty()) return;
        QJsonObject o = QJsonObject::fromVariantMap(h);
        o["topic"] = topic;
        const QByteArray line = "data: " + json(QJsonDocument(o)) + "\n\n";
        for (QTcpSocket* c : m_sseClients) c->write(line);
        ++m_streamed;
    }

private:
    void accept() {
        QTcpSocket* sock = m_server.nextPendingConnection();
        QObject::connect(sock, &QTcpSocket::readyRead, [this, sock]() { serve(sock); });
        QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }

    void serve(QTcpSocket* sock) {
        const QByteArray req = sock->readAll();
        const int sp1 = req.indexOf(' ');
        const int sp2 = req.indexOf(' ', sp1 + 1);
        if (sp1 < 0 || sp2 < 0) { sock->disconnectFromHost(); return; }
        const QUrl url(QString::fromUtf8(req.mid(sp1 + 1, sp2 - sp1 - 1)));
        const QString path = url.path();
        const QUrlQuery query(url);
        ++m_requests;

        auto sendJson = [&](const QJsonValue& v) {
            const QJsonDocument doc = v.isArray() ? QJsonDocument(v.toArray())
                                                  : QJsonDocument(v.toObject());
            sock->write(httpResponse(200, "application/json", json(doc)));
            sock->disconnectFromHost();
        };

        if (path == "/api/stream") {
            // Server-Sent Events: keep the socket open and push as data arrives.
            QByteArray h = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
                           "Cache-Control: no-cache\r\nAccess-Control-Allow-Origin: *\r\n"
                           "Connection: keep-alive\r\n\r\n";
            sock->write(h);
            sock->write(": connected\n\n");
            sock->flush();
            m_sseClients.append(sock);
            QObject::connect(sock, &QTcpSocket::disconnected, [this, sock]() {
                m_sseClients.removeAll(sock);
            });
            return;                        // do not close
        }

        QSqlQuery q(m_db);
        if (path == "/" || path == "/api" || path == "/api/") {
            QJsonObject o{
                {"service", "tpws"},
                {"endpoints", QJsonArray{"/api/health", "/api/structures", "/api/sensors",
                                         "/api/events", "/api/anomalies", "/api/features",
                                         "/api/waveform", "/api/stream"}}};
            sendJson(o);
        } else if (path == "/api/health") {
            auto count = [&](const char* t) {
                QSqlQuery c(m_db);
                return c.exec(QString("SELECT COUNT(*) FROM %1").arg(t)) && c.next()
                           ? c.value(0).toLongLong() : -1;
            };
            QJsonObject o{
                {"service", "tpws"}, {"status", "ok"},
                {"time", QDateTime::currentDateTime().toString(Qt::ISODate)},
                {"structures", count("inv_structures")}, {"sensors", count("inv_sensors")},
                {"events", count("anomaly_events")}, {"anomalies", count("events")},
                {"features", count("saf_index")},
                {"streamClients", m_sseClients.size()}};
            sendJson(o);
        } else if (path == "/api/structures") {
            q.exec("SELECT object_id, name, lat, lon, description FROM inv_structures ORDER BY object_id");
            sendJson(rowsToJson(q));
        } else if (path == "/api/sensors") {
            q.exec("SELECT s.object_id, s.sensor_id, s.model, s.location,"
                   " n.last_seen_ms, n.last_health, n.last_warning"
                   " FROM inv_sensors s LEFT JOIN sensors n"
                   " ON n.object=s.object_id AND n.sensor=s.sensor_id"
                   " ORDER BY s.object_id, s.sensor_id");
            sendJson(rowsToJson(q));
        } else if (path == "/api/events") {
            q.prepare("SELECT event_id, object, t_start_ms, t_end_ms, status, severity,"
                      " anomaly_types, sensor_count, shf_count, review"
                      " FROM anomaly_events ORDER BY t_start_ms DESC LIMIT ?");
            q.bindValue(0, clampLimit(query, 50, 1000));
            q.exec();
            sendJson(rowsToJson(q));
        } else if (path == "/api/anomalies") {
            q.prepare("SELECT shf_id, object, sensor, axis, type, severity, status,"
                      " t_start_ms, t_end_ms, duration_s, max_value, confidence, review"
                      " FROM events ORDER BY t_start_ms DESC LIMIT ?");
            q.bindValue(0, clampLimit(query, 50, 1000));
            q.exec();
            sendJson(rowsToJson(q));
        } else if (path == "/api/features") {
            QString sql = "SELECT object, sensor, axis, t_ms, rms, max_amp, dom_freq,"
                          " health, anomaly_type, warning FROM saf_index WHERE 1=1";
            const QString obj = query.queryItemValue("object");
            const QString sen = query.queryItemValue("sensor");
            const QString ax  = query.queryItemValue("axis");
            if (!obj.isEmpty()) sql += " AND object=" + QString::number(obj.toInt());
            if (!sen.isEmpty()) sql += " AND sensor=" + QString::number(sen.toInt());
            if (!ax.isEmpty())  sql += " AND axis="   + QString::number(ax.toInt());
            sql += " ORDER BY t_ms DESC LIMIT " + QString::number(clampLimit(query, 200, 5000));
            q.exec(sql);
            sendJson(rowsToJson(q));
        } else if (path == "/api/waveform") {
            // Serve recorded waveforms straight from the miniSEED archive
            // (the fdsnws-dataselect role), decimated for plotting in a page.
            const int obj = query.queryItemValue("object").toInt();
            const int sen = query.queryItemValue("sensor").toInt();
            const qlonglong t0 = query.queryItemValue("start").toLongLong();
            const qlonglong t1 = query.queryItemValue("end").toLongLong();
            const int maxPts = clampLimit(query, 1000, 20000);
            if (m_tdsRoot.isEmpty() || obj <= 0 || t1 <= t0) {
                sock->write(httpResponse(400, "application/json",
                    "{\"error\":\"need object, start, end (ms); archive must be configured\"}"));
                sock->disconnectFromHost();
                return;
            }
            QJsonObject out{{"object", obj}, {"sensor", sen},
                            {"start", t0}, {"end", t1}};
            const char* axes[3] = {"x", "y", "z"};
            double rate = 0.0;
            for (int c = 0; c < 3; ++c) {
                const tp::mseed::Trace tr =
                    tp::mseed::readTdsChannel(m_tdsRoot.toStdString(),
                                              tp::mseed::sourceId(obj, sen, c));
                QJsonArray pts;
                if (tr.sampleRate > 0 && !tr.samples.empty()) {
                    rate = tr.sampleRate;
                    const double dtMs = 1000.0 / tr.sampleRate;
                    long i0 = long((t0 - tr.startTimeMs) / dtMs);
                    long i1 = long((t1 - tr.startTimeMs) / dtMs);
                    i0 = qMax(0L, i0);
                    i1 = qMin(long(tr.samples.size()), i1);
                    const long span = i1 - i0;
                    if (span > 0) {
                        const long step = qMax(1L, span / maxPts);
                        for (long i = i0; i < i1; i += step)
                            pts.append(tr.samples[i] / 10000.0);   // counts -> gal
                    }
                }
                out[axes[c]] = pts;
            }
            out["rate"] = rate;
            sendJson(out);
        } else {
            sock->write(httpResponse(404, "application/json", "{\"error\":\"not found\"}"));
            sock->disconnectFromHost();
        }
    }

    QString m_dbPath, m_tdsRoot, m_bind;
    quint16 m_port;
    QSqlDatabase m_db;
    QTcpServer m_server;
    QTimer m_statsTimer;
    QList<QTcpSocket*> m_sseClients;
    quint64 m_requests = 0, m_streamed = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpws");

    tp::Config cfg;
    cfg.load("tpws");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse web service (REST + live stream)");
    parser.addHelpOption();
    QCommandLineOption dbOpt("db", "SQLite database (read-only)", "path",
                             cfg.str("database.path", "terrapulse.db"));
    QCommandLineOption portOpt("port", "TCP port", "n", QString::number(cfg.integer("ws.port", 8080)));
    QCommandLineOption bindOpt("bind", "Bind address", "addr", cfg.str("ws.bind", "0.0.0.0"));
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host (for the live stream)", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption tdsOpt("tds", "miniSEED archive root (serves /api/waveform)", "dir",
                              qEnvironmentVariable("TP_TDS", cfg.str("archive.tds", "")));
    parser.addOptions({dbOpt, portOpt, bindOpt, masterOpt, tdsOpt});
    parser.process(app);

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpws";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = "production";
    settings.subscriptions = {"saf.", "shf.", "evt.", "qc.", "wfp."};
    settings.sohIntervalSeconds = 0;    // read-only: never publishes, not even SOH

    WsApplication ws(std::move(settings),
                     parser.value(dbOpt), parser.value(tdsOpt), parser.value(bindOpt),
                     quint16(parser.value(portOpt).toUShort()));
    return ws.exec();
}
