// tpevtlog — writes the EVOLUTION of each anomaly event to a file (SeisComp
// scevtlog). The database holds the current state; this keeps the history of how
// an event developed — every update, in order. Together with the operator journal
// it is the audit trail: what the system saw, when, and what changed.
//
// One file per event under <dir>/<YYYY>/<MM>/event_<id>.log, appended.
//
//   tpevtlog [--master host] [--dir var/events]

#include "terrapulse/client/application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cstdio>

namespace {

class EvtLogApplication : public tp::client::Application {
public:
    EvtLogApplication(tp::client::ApplicationSettings settings, QString dir)
        : Application(std::move(settings)), m_root(std::move(dir)) {}

    bool init() override {
        if (!Application::init()) return false;
        std::printf("[tpevtlog] evt/jrnl <- %s   -> %s/<yyyy>/<MM>/event_<id>.log\n",
                    messagingUrl().toUtf8().constData(), m_root.toUtf8().constData());
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["written"] = static_cast<qulonglong>(m_written);
        return c;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        const QString type = h.value("type").toString();

        if (topic.startsWith("evt.") && type == "event") {
            logLine(h.value("eventId").toULongLong(),
                    QString("%1  object=%2 status=%3 severity=%4 types=[%5] "
                            "sensors=%6 detections=%7")
                        .arg(h.value("op").toString().toUpper(),
                             QString::number(h.value("objectId").toUInt()),
                             h.value("statusName").toString(),
                             h.value("severityName").toString(),
                             h.value("anomalyTypes").toString(),
                             QString::number(h.value("sensorCount").toInt()),
                             QString::number(h.value("shfCount").toInt())));
        } else if (topic.startsWith("jrnl.") && type == "journal") {
            logLine(h.value("eventId").toULongLong(),
                    QString("OPERATOR  action=%1 by=%2 note=%3")
                        .arg(h.value("action").toString(),
                             h.value("operator").toString(),
                             h.value("note").toString()));
        }
    }

private:
    void logLine(qulonglong eventId, const QString& line) {
        const QDateTime now = QDateTime::currentDateTime();
        const QString dir = QString("%1/%2/%3").arg(m_root, now.toString("yyyy"), now.toString("MM"));
        QDir().mkpath(dir);
        QFile f(QString("%1/event_%2.log").arg(dir).arg(eventId));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
        QTextStream out(&f);
        out << now.toString(Qt::ISODate) << "  " << line << "\n";
        ++m_written;
    }

    QString m_root;
    quint64 m_written = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpevtlog");

    tp::Config cfg;
    cfg.load("tpevtlog");

    QCommandLineParser parser;
    parser.setApplicationDescription("Log anomaly-event evolution to files");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption queueOpt("queue", "Queue: production | playback", "name",
                                cfg.str("connection.queue", "production"));
    QCommandLineOption dirOpt("dir", "Output directory", "path",
                              cfg.str("evtlog.dir", "var/events"));
    parser.addOptions({masterOpt, queueOpt, dirOpt});
    parser.process(app);

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpevtlog";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = parser.value(queueOpt);
    // Operator decisions belong in the same history as the automatic evolution.
    settings.subscriptions = {"evt.", "jrnl."};
    settings.sohIntervalSeconds = 2;

    EvtLogApplication evtlog(std::move(settings), parser.value(dirOpt));
    return evtlog.exec();
}
