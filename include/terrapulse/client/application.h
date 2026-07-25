#pragma once

#include "config/Config.h"
#include "terrapulse/client/monitor.h"
#include "terrapulse/client/queue.h"
#include "terrapulse/core/message.h"
#include "terrapulse/messaging/connection.h"
#include "terrapulse/messaging/message.h"
#include "terrapulse/version.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <cstddef>
#include <functional>
#include <memory>
#include <set>
#include <QString>
#include <utility>

namespace tp::client {

// Common base settings for TerraPulse modules. This mirrors the SeisComp idea
// of every module starting from one application/config layer, while keeping the
// current TerraPulse daemons small and portable.
struct ApplicationSettings {
    QString moduleName;
    QString root;
    QString masterHost = "127.0.0.1";
    QString queue = "production";
    QString databaseUri;
    QString recordStreamUri;
    QString author = "@module@";
    QString agencyId = "TP";
    bool messagingEnabled = true;
    bool databaseEnabled = false;
    bool recordStreamEnabled = false;
    bool inventoryEnabled = false;
    bool startStopMessagesEnabled = false;
    int sohIntervalSeconds = 60;
    // How often the bus is drained. Raw-rate consumers want this tight so
    // latency stays low; summary consumers can afford the default.
    int pollIntervalMs = 50;
    // Store-and-forward: if > 0, the publisher buffers this many messages while
    // the broker link is down and resends them on reconnect (acquisition never
    // loses samples across a broker restart). 0 = publish straight through.
    std::size_t storeForwardCap = 0;
    QStringList subscriptions;
};

enum class ApplicationStatus {
    Started,
    Finished
};

struct ApplicationStatusMessage {
    QString module;
    QString username;
    ApplicationStatus status = ApplicationStatus::Started;
};

struct Notification {
    enum Type {
        Object,
        Disconnect,
        Reconnect,
        Close,
        Timeout,
        AcquisitionFinished,
        StateOfHealth
    };

    int type = Object;
    std::shared_ptr<tp::core::BaseObject> object;
};

class Application {
public:
    explicit Application(ApplicationSettings settings)
        : m_settings(std::move(settings)) {}
    virtual ~Application() = default;

    const ApplicationSettings& settings() const { return m_settings; }
    const Config& config() const { return m_config; }
    Config& config() { return m_config; }
    const std::set<std::string>& subscribedGroups() const { return m_subscriptions; }
    ObjectMonitor& inputMonitor() { return m_inputMonitor; }
    ObjectMonitor& outputMonitor() { return m_outputMonitor; }

    const QString& databaseUri() const { return m_settings.databaseUri; }
    const QString& recordStreamUri() const { return m_settings.recordStreamUri; }
    const QString& messagingUrl() const { return m_messagingUrl; }

    void loadConfig() {
        m_config.load(m_settings.moduleName, m_settings.root);
    }

    virtual bool init();
    virtual int exec();
    virtual void exit(int returnCode);

    void addMessagingSubscription(const std::string& group);
    void setMessagingEnabled(bool enabled) { m_settings.messagingEnabled = enabled; }
    void setDatabaseEnabled(bool enabled) { m_settings.databaseEnabled = enabled; }
    void setRecordStreamEnabled(bool enabled) { m_settings.recordStreamEnabled = enabled; }
    void setStartStopMessagesEnabled(bool enabled) { m_settings.startStopMessagesEnabled = enabled; }

    void sendNotification(Notification notification);
    bool waitEvent(Notification* out = nullptr);

    // Publish through the broker. Available once init() has run.
    void publish(const tp::messaging::Message& message);

    // Convenience: build and publish in one call, so modules deal only with a
    // topic and a header map.
    void publish(const std::string& topic, const QVariantMap& header);

    // One-shot pattern (tools like tpinv/tpjournal): after the broker's
    // subscription has propagated (PUB/SUB is a slow joiner), run `emit_`, let the
    // messages drain, then quit the event loop with the given code.
    void publishThenQuit(std::function<void()> emit_, int code = 0,
                         int settleMs = 1600, int drainMs = 400);
    tp::messaging::Publisher*  publisher()  { return m_publisher.get(); }
    tp::messaging::Subscriber* subscriber() { return m_subscriber.get(); }

    // Counters a module wants in its heartbeat; merged into the SOH message the
    // base class sends, so no module has to build one itself.
    virtual QVariantMap sohCounters() { return {}; }

protected:
    virtual bool run();
    virtual void done();

    // Called around a drained batch of messages (only when at least one arrived),
    // so a storage module can wrap the batch in a single database transaction.
    virtual void beforeBatch() {}
    virtual void afterBatch()  {}

    // Called with the untouched message before anything is decoded. Return true
    // to consume it, which also skips the decode — a relay or archiver overrides
    // this so bytes pass through exactly as received.
    virtual bool handleRawMessage(const tp::messaging::Message& message);

    // Called for every message on a subscribed group. `header` is the decoded
    // CBOR header; `topic` carries the group prefix and routing ids.
    virtual void handleMessage(const QString& topic, const QVariantMap& header);

    // Typed-object entry point, kept for the future `datamodel` layer.
    virtual void handleMessage(tp::core::Message* message);

    virtual void handleTimeout();
    virtual void handleSOH();
    virtual void handleDisconnect();
    virtual void handleReconnect();
    virtual bool handleClose();

private:
    void pumpMessages();          // drain the subscriber, dispatch handleMessage
    void sendHeartbeat();         // publish soh.<module> with sohCounters()

    ApplicationSettings m_settings;
    Config m_config;
    QString m_messagingUrl;
    int m_returnCode = 0;
    bool m_quit = false;
    std::set<std::string> m_subscriptions;
    ThreadedQueue<Notification> m_notifications{1024};
    ObjectMonitor m_inputMonitor{60};
    ObjectMonitor m_outputMonitor{60};

    // Broker connection, created in init(): subscriber on the master's output,
    // publisher on its input — the same asymmetry every module already uses.
    std::unique_ptr<tp::messaging::Publisher>  m_publisher;
    std::unique_ptr<tp::messaging::Subscriber> m_subscriber;
    QTimer  m_pollTimer;
    QTimer  m_sohTimer;
    QTimer  m_sfTimer;        // services store-and-forward when enabled
    qint64  m_startMs = 0;
    quint64 m_messagesIn = 0;
};

} // namespace tp::client
