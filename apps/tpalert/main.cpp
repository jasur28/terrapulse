// tpalert — turns anomalies into notifications (SeisComp scalert analog). Listens
// for anomaly events and runs an operator-configured external command, so alerting
// (e-mail, SMS, Telegram, a siren, a ticket) is a matter of configuration rather
// than code. Rate-limited per structure so a long event cannot spam.
//
// The command string may contain placeholders:
//   {event} {object} {severity} {types} {status} {sensors} {time}
//
//   tpalert [--master host] [--command "notify.bat {object} {severity}"]
//           [--min-severity 2] [--cooldown-sec 60] [--dry-run]

#include "terrapulse/client/application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QHash>
#include <QProcess>
#include <QStringList>
#include <cstdio>

namespace {

class AlertApplication : public tp::client::Application {
public:
    AlertApplication(tp::client::ApplicationSettings settings, QString command,
                     int minSeverity, qint64 cooldownMs, bool dryRun)
        : Application(std::move(settings)), m_command(std::move(command)),
          m_minSeverity(minSeverity), m_cooldownMs(cooldownMs), m_dryRun(dryRun) {}

    bool init() override {
        if (!Application::init()) return false;
        std::printf("[tpalert] evt <- %s   minSeverity=%d cooldown=%llds%s\n",
                    messagingUrl().toUtf8().constData(), m_minSeverity,
                    static_cast<long long>(m_cooldownMs / 1000), m_dryRun ? " (dry-run)" : "");
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["seen"]       = static_cast<qulonglong>(m_seen);
        c["fired"]      = static_cast<qulonglong>(m_fired);
        c["suppressed"] = static_cast<qulonglong>(m_suppressed);
        return c;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (!topic.startsWith("evt.")) return;
        if (h.value("type").toString() != "event") return;
        ++m_seen;

        const int     sev    = h.value("severity").toInt();
        const quint32 obj    = h.value("objectId").toUInt();
        const quint64 evId   = h.value("eventId").toULongLong();
        const int     status = h.value("status").toInt();

        if (sev < m_minSeverity) return;
        if (status == 1) return;                       // resolved: nothing to alert
        // Alert once per event, and again only if it escalates.
        if (m_alertedSeverity.contains(evId) && m_alertedSeverity[evId] >= sev) return;

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastAlert.contains(obj) && now - m_lastAlert[obj] < m_cooldownMs) {
            ++m_suppressed;
            return;
        }

        m_alertedSeverity[evId] = sev;
        m_lastAlert[obj] = now;

        QString line = m_command;
        line.replace("{event}",    QString::number(evId))
            .replace("{object}",   QString::number(obj))
            .replace("{severity}", h.value("severityName").toString())
            .replace("{types}",    h.value("anomalyTypes").toString())
            .replace("{status}",   h.value("statusName").toString())
            .replace("{sensors}",  QString::number(h.value("sensorCount").toInt()))
            .replace("{time}",     QDateTime::fromMSecsSinceEpoch(
                                       h.value("tStart").toLongLong()).toString(Qt::ISODate));

        ++m_fired;
        std::printf("[tpalert] ALERT object=%u event=%llu severity=%s types=%s%s\n",
                    obj, static_cast<unsigned long long>(evId),
                    h.value("severityName").toString().toUtf8().constData(),
                    h.value("anomalyTypes").toString().toUtf8().constData(),
                    m_dryRun ? "  (dry-run)" : "");
        if (!m_command.isEmpty()) {
            std::printf("[tpalert]   run: %s\n", line.toUtf8().constData());
            if (!m_dryRun && !QProcess::startDetached(QString("cmd"),
                                                      QStringList() << "/c" << line))
                std::printf("[tpalert]   ! failed to start command\n");
        } else {
            std::printf("[tpalert]   (no alert.command configured — logging only)\n");
        }
        std::fflush(stdout);
    }

private:
    QString m_command;
    int     m_minSeverity;
    qint64  m_cooldownMs;
    bool    m_dryRun;

    QHash<quint32, qint64> m_lastAlert;        // objectId -> when we last alerted
    QHash<quint64, int>    m_alertedSeverity;  // eventId -> severity already alerted on
    quint64 m_seen = 0, m_fired = 0, m_suppressed = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpalert");

    tp::Config cfg;
    cfg.load("tpalert");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse alerting");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption queueOpt("queue", "Queue: production | playback", "name",
                                cfg.str("connection.queue", "production"));
    QCommandLineOption cmdOpt("command", "Command to run on alert (placeholders allowed)", "cmd",
                              cfg.str("alert.command", ""));
    QCommandLineOption sevOpt("min-severity", "Minimum severity 0=LOW..3=CRITICAL", "n",
                              QString::number(cfg.integer("alert.minSeverity", 2)));
    QCommandLineOption coolOpt("cooldown-sec", "Minimum seconds between alerts per structure", "s",
                               QString::number(cfg.integer("alert.cooldownSec", 60)));
    QCommandLineOption dryOpt("dry-run", "Log what would run, do not execute");
    parser.addOptions({masterOpt, queueOpt, cmdOpt, sevOpt, coolOpt, dryOpt});
    parser.process(app);

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpalert";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = parser.value(queueOpt);
    settings.subscriptions = {"evt."};          // grouped events: one alert per event
    settings.sohIntervalSeconds = 2;

    AlertApplication alert(std::move(settings),
                           parser.value(cmdOpt),
                           parser.value(sevOpt).toInt(),
                           qMax(0, parser.value(coolOpt).toInt()) * 1000LL,
                           parser.isSet(dryOpt) || cfg.boolean("alert.dryRun", false));
    return alert.exec();
}
