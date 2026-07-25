// tpevent — groups individual anomaly detections (SHF) into structure-level
// anomaly EVENTS (like SeisComp's scevent grouping picks into an event). One open
// event per monitored object: it opens on the first anomaly, absorbs further
// anomalies (across the object's sensors/axes) while they keep arriving, and
// closes (RESOLVED) after a quiet period. Each change is published as an `evt.`
// notifier and persisted by tpmaster (write-before-notify). No GUI.
//
//   tpevent [--master host] [--queue production|playback] [--quiet-sec 15]

#include "terrapulse/client/application.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <cstdio>
#include <string>

namespace {

QString severityName(int s) {
    switch (s) { case 3: return "CRITICAL"; case 2: return "HIGH";
                 case 1: return "MEDIUM";   default: return "LOW"; }
}
QString anomalyName(int t) {
    switch (t) { case 1: return "Vibration"; case 2: return "Resonance";
                 case 3: return "Crack"; case 4: return "Settlement";
                 case 5: return "Overload"; default: return "None"; }
}

struct Event {
    quint64        eventId = 0;
    quint32        objectId = 0;
    qint64         tStart = 0, tLast = 0;
    int            severity = 0;
    int            shfCount = 0;
    int            status = 0;          // 0 ACTIVE, 1 RESOLVED
    QSet<int>      types;
    QSet<quint32>  sensors;
};

class EventApplication : public tp::client::Application {
public:
    EventApplication(tp::client::ApplicationSettings settings, qint64 quietMs)
        : Application(std::move(settings)), m_quietMs(quietMs) {}

    bool init() override {
        if (!Application::init()) return false;
        // Closing a quiet event is this module's own cadence.
        QObject::connect(&m_sweepTimer, &QTimer::timeout, [this]() { sweep(); });
        m_sweepTimer.start(1000);

        std::printf("[tpevent] shf <- %s   evt -> broker   quiet=%llds\n",
                    messagingUrl().toUtf8().constData(),
                    static_cast<long long>(m_quietMs / 1000));
        std::fflush(stdout);
        return true;
    }

    QVariantMap sohCounters() override {
        QVariantMap c;
        c["shfIn"]  = static_cast<qulonglong>(m_shfIn);
        c["opened"] = static_cast<qulonglong>(m_opened);
        c["closed"] = static_cast<qulonglong>(m_closed);
        c["active"] = m_open.size();
        return c;
    }

protected:
    void handleMessage(const QString& topic, const QVariantMap& h) override {
        if (!topic.startsWith("shf.")) return;
        ++m_shfIn;

        const quint32 obj = h.value("objectId").toUInt();
        const qint64  t   = h.value("anomalyStartTime").toLongLong();
        const int     sev = h.value("severityLevel").toInt();
        const int     typ = h.value("anomalyType").toInt();
        const quint32 sen = h.value("sensorId").toUInt();
        const qint64  now = QDateTime::currentMSecsSinceEpoch();

        Event& e = m_open[obj];
        const bool isNew = e.eventId == 0;
        if (isNew) {
            e.eventId  = static_cast<quint64>(t > 0 ? t : now);
            e.objectId = obj;
            e.tStart   = t > 0 ? t : now;
            e.status   = 0;
            ++m_opened;
        }
        e.tLast = now;
        e.severity = qMax(e.severity, sev);
        e.types.insert(typ);
        e.sensors.insert(sen);
        ++e.shfCount;
        publishEvent(e, isNew ? "add" : "update");
    }

    void handleSOH() override {
        std::printf("[tpevent] shfIn=%llu opened=%llu closed=%llu active=%d\n",
                    static_cast<unsigned long long>(m_shfIn),
                    static_cast<unsigned long long>(m_opened),
                    static_cast<unsigned long long>(m_closed), m_open.size());
        std::fflush(stdout);
    }

private:
    void publishEvent(const Event& e, const char* op) {
        QStringList typeNames;
        for (int t : e.types) if (t != 0) typeNames << anomalyName(t);
        QVariantMap h;
        h["v"] = 1; h["type"] = "event"; h["op"] = op;
        h["eventId"]      = static_cast<qulonglong>(e.eventId);
        h["objectId"]     = e.objectId;
        h["tStart"]       = static_cast<qlonglong>(e.tStart);
        h["tEnd"]         = static_cast<qlonglong>(e.status == 1 ? e.tLast : 0);
        h["status"]       = e.status;
        h["statusName"]   = e.status == 1 ? "RESOLVED" : "ACTIVE";
        h["severity"]     = e.severity;
        h["severityName"] = severityName(e.severity);
        h["anomalyTypes"] = typeNames.join(", ");
        h["sensorCount"]  = e.sensors.size();
        h["shfCount"]     = e.shfCount;
        publish("evt." + std::to_string(e.eventId), h);
    }

    // Close events that have gone quiet.
    void sweep() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_open.begin(); it != m_open.end(); ) {
            if (now - it->tLast > m_quietMs) {
                it->status = 1;                 // RESOLVED
                publishEvent(*it, "update");
                ++m_closed;
                it = m_open.erase(it);
            } else {
                ++it;
            }
        }
    }

    QHash<quint32, Event> m_open;      // objectId -> currently open event
    quint64 m_opened = 0, m_closed = 0, m_shfIn = 0;
    QTimer  m_sweepTimer;
    qint64  m_quietMs;
};

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpevent");

    tp::Config cfg;
    cfg.load("tpevent");

    QCommandLineParser parser;
    parser.setApplicationDescription("TerraPulse anomaly-event grouping");
    parser.addHelpOption();
    QCommandLineOption masterOpt({"m", "master"}, "tpmaster host", "host",
                                 cfg.str("connection.server", "127.0.0.1"));
    QCommandLineOption queueOpt("queue", "Queue: production | playback", "name",
                                cfg.str("connection.queue", "production"));
    QCommandLineOption quietOpt("quiet-sec", "Close an event after this many seconds without new anomalies",
                                "sec", QString::number(cfg.integer("event.quietSec", 15)));
    parser.addOptions({masterOpt, queueOpt, quietOpt});
    parser.process(app);

    tp::client::ApplicationSettings settings;
    settings.moduleName    = "tpevent";
    settings.masterHost    = parser.value(masterOpt);
    settings.queue         = parser.value(queueOpt);
    settings.subscriptions = {"shf."};
    settings.sohIntervalSeconds = 2;

    EventApplication ev(std::move(settings),
                        qMax(2, parser.value(quietOpt).toInt()) * 1000LL);
    return ev.exec();
}
