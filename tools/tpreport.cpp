// tpreport — produces an operator report (SeisComp scbulletin). Turns the archive
// into something a client or an inspector can read: an HTML report (print it to
// PDF from any browser), a PDF directly, and KML so the structures and their
// status can be opened on a map.
//
//   tpreport --db terrapulse.db --out report.html [--days 7]
//   tpreport --db terrapulse.db --out report.pdf  --pdf
//   tpreport --db terrapulse.db --out sites.kml   --kml

#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTextDocument>
#include <QTextStream>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <cstdio>

namespace {

QString esc(const QString& s) {
    QString o = s;
    o.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
    return o;
}

QString severityLabel(int s) {
    switch (s) { case 3: return "CRITICAL"; case 2: return "HIGH";
                 case 1: return "MEDIUM";   default: return "LOW"; }
}
QString severityColour(int s) {
    switch (s) { case 3: return "#FF1744"; case 2: return "#FF6D00";
                 case 1: return "#FFD600"; default: return "#00C853"; }
}

QString buildHtml(QSqlDatabase& db, int days) {
    const qint64 cut = QDateTime::currentMSecsSinceEpoch() - qint64(days) * 86400000LL;
    QString h;
    QTextStream o(&h);

    o << "<html><head><meta charset='utf-8'><title>TerraPulse report</title><style>"
         "body{font-family:Segoe UI,Arial,sans-serif;margin:32px;color:#222}"
         "h1{margin:0 0 4px 0}h2{margin-top:28px;border-bottom:2px solid #16213E;padding-bottom:4px}"
         "table{border-collapse:collapse;width:100%;margin-top:8px}"
         "th{background:#16213E;color:#fff;text-align:left;padding:6px 8px;font-size:13px}"
         "td{border-bottom:1px solid #ddd;padding:6px 8px;font-size:13px}"
         ".sub{color:#666;font-size:13px}.pill{padding:2px 8px;border-radius:10px;color:#000;font-weight:bold}"
         "</style></head><body>";
    o << "<h1>TerraPulse — structural monitoring report</h1>";
    o << "<div class='sub'>Generated " << QDateTime::currentDateTime().toString(Qt::ISODate)
      << " &middot; reporting period: last " << days << " day(s)</div>";

    QSqlQuery q(db);

    // ── Monitored structures and their current state ──
    o << "<h2>Monitored structures</h2><table><tr><th>ID</th><th>Name</th><th>Description</th>"
         "<th>Location</th><th>Sensors</th></tr>";
    q.exec("SELECT s.object_id, s.name, s.description, s.lat, s.lon,"
           " (SELECT COUNT(*) FROM inv_sensors v WHERE v.object_id=s.object_id)"
           " FROM inv_structures s ORDER BY s.object_id");
    while (q.next())
        o << "<tr><td>" << q.value(0).toInt() << "</td><td><b>" << esc(q.value(1).toString())
          << "</b></td><td>" << esc(q.value(2).toString()) << "</td><td>"
          << q.value(3).toDouble() << ", " << q.value(4).toDouble() << "</td><td>"
          << q.value(5).toInt() << "</td></tr>";
    o << "</table>";

    // ── Sensor health ──
    o << "<h2>Sensor health</h2><table><tr><th>Structure</th><th>Sensor</th><th>Model</th>"
         "<th>Location</th><th>Health</th><th>Last seen</th></tr>";
    q.exec("SELECT v.object_id, v.sensor_id, v.model, v.location, n.last_health, n.last_seen_ms"
           " FROM inv_sensors v LEFT JOIN sensors n"
           " ON n.object=v.object_id AND n.sensor=v.sensor_id"
           " ORDER BY v.object_id, v.sensor_id");
    while (q.next()) {
        const double health = q.value(4).toDouble();
        const qint64 seen   = q.value(5).toLongLong();
        o << "<tr><td>" << q.value(0).toInt() << "</td><td>" << q.value(1).toInt()
          << "</td><td>" << esc(q.value(2).toString()) << "</td><td>" << esc(q.value(3).toString())
          << "</td><td>" << (q.value(4).isNull() ? QString("—") : QString::number(health, 'f', 3))
          << "</td><td>"
          << (seen > 0 ? QDateTime::fromMSecsSinceEpoch(seen).toString(Qt::ISODate) : QString("—"))
          << "</td></tr>";
    }
    o << "</table>";

    // ── Anomaly events in the period ──
    o << "<h2>Anomaly events</h2><table><tr><th>Event</th><th>Structure</th><th>Severity</th>"
         "<th>Types</th><th>Sensors</th><th>Detections</th><th>Started</th><th>Status</th>"
         "<th>Review</th></tr>";
    q.prepare("SELECT event_id,object,severity,anomaly_types,sensor_count,shf_count,"
              "t_start_ms,status,review FROM anomaly_events WHERE t_start_ms>=?"
              " ORDER BY t_start_ms DESC");
    q.bindValue(0, cut);
    q.exec();
    int events = 0;
    while (q.next()) {
        ++events;
        const int sev = q.value(2).toInt();
        const int rev = q.value(8).toInt();
        o << "<tr><td>" << q.value(0).toLongLong() << "</td><td>" << q.value(1).toInt()
          << "</td><td><span class='pill' style='background:" << severityColour(sev) << "'>"
          << severityLabel(sev) << "</span></td><td>" << esc(q.value(3).toString())
          << "</td><td>" << q.value(4).toInt() << "</td><td>" << q.value(5).toInt()
          << "</td><td>" << QDateTime::fromMSecsSinceEpoch(q.value(6).toLongLong()).toString(Qt::ISODate)
          << "</td><td>" << (q.value(7).toInt() == 1 ? "RESOLVED" : "ACTIVE")
          << "</td><td>" << (rev == 1 ? "CONFIRMED" : rev == 2 ? "REJECTED" : "AUTO")
          << "</td></tr>";
    }
    if (events == 0) o << "<tr><td colspan='9'>No anomaly events in this period.</td></tr>";
    o << "</table>";

    // ── Natural frequency per channel: the stiffness-loss baseline ──
    o << "<h2>Natural frequency by channel</h2>"
         "<div class='sub'>A sustained drop in these values indicates loss of stiffness.</div>"
         "<table><tr><th>Structure</th><th>Sensor</th><th>Axis</th><th>Mean frequency (Hz)</th>"
         "<th>Mean health</th><th>Windows</th></tr>";
    q.prepare("SELECT object,sensor,axis,AVG(dom_freq),AVG(health),COUNT(*) FROM saf_index"
              " WHERE t_ms>=? GROUP BY object,sensor,axis ORDER BY object,sensor,axis");
    q.bindValue(0, cut);
    q.exec();
    while (q.next()) {
        const char* ax = q.value(2).toInt() == 0 ? "X" : q.value(2).toInt() == 1 ? "Y" : "Z";
        o << "<tr><td>" << q.value(0).toInt() << "</td><td>" << q.value(1).toInt()
          << "</td><td>" << ax << "</td><td>" << QString::number(q.value(3).toDouble(), 'f', 3)
          << "</td><td>" << QString::number(q.value(4).toDouble(), 'f', 3)
          << "</td><td>" << q.value(5).toInt() << "</td></tr>";
    }
    o << "</table>";

    o << "<h2>Operator actions</h2><table><tr><th>When</th><th>Event</th><th>Action</th>"
         "<th>Operator</th><th>Note</th></tr>";
    q.prepare("SELECT t_ms,event_shf_id,action,operator,note FROM journal WHERE t_ms>=?"
              " ORDER BY t_ms DESC LIMIT 200");
    q.bindValue(0, cut);
    q.exec();
    int jr = 0;
    while (q.next()) {
        ++jr;
        o << "<tr><td>" << QDateTime::fromMSecsSinceEpoch(q.value(0).toLongLong()).toString(Qt::ISODate)
          << "</td><td>" << q.value(1).toLongLong() << "</td><td>" << esc(q.value(2).toString())
          << "</td><td>" << esc(q.value(3).toString()) << "</td><td>" << esc(q.value(4).toString())
          << "</td></tr>";
    }
    if (jr == 0) o << "<tr><td colspan='5'>No operator actions in this period.</td></tr>";
    o << "</table>";

    o << "<p class='sub' style='margin-top:32px'>TerraPulse &middot; automated report</p>";
    o << "</body></html>";
    return h;
}

QString buildKml(QSqlDatabase& db) {
    QString k;
    QTextStream o(&k);
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>\n"
         "  <name>TerraPulse monitored structures</name>\n";
    QSqlQuery q(db);
    q.exec("SELECT s.object_id, s.name, s.description, s.lat, s.lon,"
           " (SELECT COUNT(*) FROM anomaly_events e WHERE e.object=s.object_id AND e.status=0)"
           " FROM inv_structures s WHERE s.lat IS NOT NULL ORDER BY s.object_id");
    while (q.next()) {
        const int open = q.value(5).toInt();
        o << "  <Placemark>\n    <name>" << esc(q.value(1).toString()) << "</name>\n"
          << "    <description>" << esc(q.value(2).toString())
          << (open > 0 ? QString(" — %1 active event(s)").arg(open) : QString(" — normal"))
          << "</description>\n"
          << "    <Point><coordinates>" << q.value(4).toDouble() << ","
          << q.value(3).toDouble() << ",0</coordinates></Point>\n  </Placemark>\n";
    }
    o << "</Document></kml>\n";
    return k;
}

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");     // render without a display
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpreport");

    QCommandLineParser parser;
    parser.setApplicationDescription("Generate a TerraPulse report (HTML / PDF / KML)");
    parser.addHelpOption();
    QCommandLineOption dbOpt("db", "SQLite database", "path", "terrapulse.db");
    QCommandLineOption outOpt("out", "Output file", "path", "report.html");
    QCommandLineOption daysOpt("days", "Reporting period in days", "n", "7");
    QCommandLineOption pdfOpt("pdf", "Write a PDF instead of HTML");
    QCommandLineOption kmlOpt("kml", "Write KML (structures for a map) instead");
    parser.addOptions({dbOpt, outOpt, daysOpt, pdfOpt, kmlOpt});
    parser.process(app);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tpreport");
    db.setDatabaseName(parser.value(dbOpt));
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if (!db.open()) {
        std::fprintf(stderr, "tpreport: cannot open '%s': %s\n",
                     parser.value(dbOpt).toUtf8().constData(),
                     db.lastError().text().toUtf8().constData());
        return 1;
    }

    const QString outPath = parser.value(outOpt);

    if (parser.isSet(kmlOpt)) {
        QFile f(outPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return 1;
        QTextStream(&f) << buildKml(db);
        std::printf("tpreport: wrote %s (KML)\n", outPath.toUtf8().constData());
        return 0;
    }

    const QString html = buildHtml(db, parser.value(daysOpt).toInt());

    if (parser.isSet(pdfOpt)) {
        QPdfWriter pdf(outPath);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setResolution(150);
        QTextDocument doc;
        doc.setHtml(html);
        doc.setPageSize(QSizeF(pdf.width(), pdf.height()));
        doc.print(&pdf);
        std::printf("tpreport: wrote %s (PDF)\n", outPath.toUtf8().constData());
        return 0;
    }

    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        std::fprintf(stderr, "tpreport: cannot write '%s'\n", outPath.toUtf8().constData());
        return 1;
    }
    QTextStream(&f) << html;
    std::printf("tpreport: wrote %s (HTML — print to PDF from a browser)\n",
                outPath.toUtf8().constData());
    return 0;
}
