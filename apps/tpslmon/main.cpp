// tpslmon — acquisition status page (SeisComp slmon2). Serves a small
// self-refreshing HTML dashboard showing which modules are alive and whether each
// sensor's data is actually arriving. Deliberately dependency-free: a maintenance
// technician on site opens a browser, no TerraPulse client needed.
//
//   tpslmon [--master host] [--port 8081]

#include "terrapulse/client/application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstdio>

namespace {

struct ModuleState { qint64 lastSeen = 0; QVariantMap counters; };
struct SensorState {
    qint64  lastSeen = 0;
    QString verdict = "-";
    double  availability = 0, latencyMs = 0, rate = 0;
    quint64 gaps = 0;
};

QString esc(const QString& s) { QString o = s; o.replace('&',"&amp;").replace('<',"&lt;"); return o; }

class HubMonApplication : public tp::client::Application {
public:
    HubMonApplication(tp::client::ApplicationSettings settings, quint16 port)
        : Application(std::move(settings)), m_port(port) {}

    bool init() override {
        if (!Application::init()) return false;

        QObject::connect(&m_server, &QTcpServer::newConnection, [this]() { serve(); });
        if (!m_server.listen(QHostAddress::Any, m_port)) {
            std::fprintf(stderr, "tpslmon: cannot listen on port %u: %s\n", m_port,
                         m_server.errorString().toUtf8().constData());
            return false;
        }
        std::printf("[tpslmon] http://localhost:%u/\n", m_port);
        std::fflush(stdout);
        return true;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (topic.startsWith("soh.")) {
            ModuleState& s = m_modules[h.value("module").toString()];
            s.lastSeen = now;
            s.counters = h.value("counters").toMap();
            if (s.counters.isEmpty()) {           // counters may be inline
                QVariantMap c = h;
                c.remove("module"); c.remove("type"); c.remove("v");
                c.remove("pid"); c.remove("uptime"); c.remove("t");
                s.counters = c;
            }
        } else if (topic.startsWith("qc.")) {
            const QString key = QString("%1/%2").arg(h.value("objectId").toUInt())
                                                .arg(h.value("sensorId").toUInt());
            SensorState& s = m_sensors[key];
            s.lastSeen     = now;
            s.verdict      = h.value("verdict").toString();
            s.availability = h.value("availability").toDouble();
            s.latencyMs    = h.value("latencyMs").toDouble();
            s.rate         = h.value("sampleRate").toDouble();
            s.gaps         = h.value("gaps").toULongLong();
        }
    }

private:
    void serve() {
        QTcpSocket* sock = m_server.nextPendingConnection();
        QObject::connect(sock, &QTcpSocket::readyRead, [this, sock]() {
            sock->readAll();
            const QByteArray body = page().toUtf8();
            QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                              "Content-Length: " + QByteArray::number(body.size()) +
                              "\r\nConnection: close\r\n\r\n";
            sock->write(resp + body);
            sock->disconnectFromHost();
        });
        QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }

    QString page() const {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QString b;
        b += "<html><head><meta charset='utf-8'><meta http-equiv='refresh' content='5'>"
             "<title>TerraPulse hub</title><style>"
             "body{font-family:Segoe UI,Arial,sans-serif;background:#1A1A2E;color:#E0E0E0;margin:24px}"
             "h1{margin:0 0 2px 0;font-size:22px}h2{margin:24px 0 6px 0;font-size:16px;color:#A0A0A0}"
             "table{border-collapse:collapse;width:100%}"
             "th{text-align:left;padding:6px 10px;background:#16213E;font-size:13px}"
             "td{padding:6px 10px;border-bottom:1px solid #22314f;font-size:13px}"
             ".up{color:#00C853;font-weight:bold}.down{color:#FF1744;font-weight:bold}"
             ".warn{color:#FFD600;font-weight:bold}.sub{color:#A0A0A0;font-size:12px}"
             "</style></head><body>";
        b += "<h1>TerraPulse — acquisition status</h1>";
        b += "<div class='sub'>" + QDateTime::currentDateTime().toString(Qt::ISODate)
           + " &middot; refreshes every 5 s</div>";

        b += "<h2>Modules</h2><table><tr><th>Module</th><th>State</th><th>Last seen</th>"
             "<th>Counters</th></tr>";
        if (m_modules.isEmpty()) b += "<tr><td colspan='4'>waiting for heartbeats…</td></tr>";
        for (auto it = m_modules.begin(); it != m_modules.end(); ++it) {
            const qint64 age = now - it->lastSeen;
            const bool up = age < 6000;
            QStringList cs;
            for (auto c = it->counters.begin(); c != it->counters.end(); ++c)
                cs << QString("%1=%2").arg(c.key(), c.value().toString());
            b += QString("<tr><td>%1</td><td class='%2'>%3</td><td>%4 s ago</td><td class='sub'>%5</td></tr>")
                    .arg(esc(it.key()), up ? "up" : "down", up ? "UP" : "DOWN")
                    .arg(age / 1000).arg(esc(cs.join("  ")));
        }
        b += "</table>";

        b += "<h2>Sensors (data quality)</h2><table><tr><th>Object/Sensor</th><th>Verdict</th>"
             "<th>Availability</th><th>Gaps</th><th>Latency</th><th>Rate</th></tr>";
        if (m_sensors.isEmpty())
            b += "<tr><td colspan='6'>no QC reports yet (is tpqc running?)</td></tr>";
        for (auto it = m_sensors.begin(); it != m_sensors.end(); ++it) {
            const QString cls = it->verdict == "GOOD" ? "up"
                              : it->verdict == "DEGRADED" ? "warn" : "down";
            b += QString("<tr><td>%1</td><td class='%2'>%3</td><td>%4%</td><td>%5</td>"
                         "<td>%6 ms</td><td>%7 Hz</td></tr>")
                    .arg(esc(it.key()), cls, esc(it->verdict))
                    .arg(it->availability, 0, 'f', 1)
                    .arg(it->gaps)
                    .arg(it->latencyMs, 0, 'f', 0)
                    .arg(it->rate, 0, 'f', 0);
        }
        b += "</table></body></html>";
        return b;
    }

    QMap<QString, ModuleState> m_modules;
    QMap<QString, SensorState> m_sensors;
    QTcpServer m_server;
    quint16    m_port;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpslmon");

    tp::Config cfg;
    cfg.load("tpslmon");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse acquisition status page");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption portOpt("port", "TCP port", "n",
                               QString::number(cfg.integer("hubmon.port", 8081)));
    parser.addOptions({masterOpt, portOpt});
    parser.process(app);

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpslmon";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = "production";
    settings.subscriptions = {"soh.", "qc."};
    // A status page must not appear in its own module table as a heartbeat source.
    settings.sohIntervalSeconds = 0;

    HubMonApplication hub(std::move(settings), quint16(parser.value(portOpt).toUShort()));
    return hub.exec();
}
