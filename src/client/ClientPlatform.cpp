#include "terrapulse/client/application.h"
#include "terrapulse/client/configdb.h"
#include "terrapulse/client/inventory.h"
#include "terrapulse/client/streamapplication.h"

#include "bus/Master.h"
#include "bus/BusMessage.h"
#include "bus/Soh.h"

#include <QDateTime>

#include <algorithm>
#include <iostream>

namespace tp::client {

void RunningAverage::push(const tp::core::Time& time, std::size_t count) {
    const qint64 now = time.toMSecsSinceEpoch();
    m_last = time;
    m_bins.emplace_back(now, count);
    const qint64 minTime = now - qint64(m_timeSpan) * 1000;
    while (!m_bins.empty() && m_bins.front().first < minTime) m_bins.pop_front();
}

int RunningAverage::count(const tp::core::Time& time) const {
    const qint64 minTime = time.toMSecsSinceEpoch() - qint64(m_timeSpan) * 1000;
    std::size_t sum = 0;
    for (const auto& item : m_bins) {
        if (item.first >= minTime) sum += item.second;
    }
    return static_cast<int>(sum);
}

double RunningAverage::value(const tp::core::Time& time) const {
    return static_cast<double>(count(time)) / std::max(1, m_timeSpan);
}

ObjectMonitor::Log* ObjectMonitor::add(const std::string& name, const std::string& channel) {
    m_tests.push_back(Test{name, channel, {}, 0, Log{m_timeSpan}});
    return &m_tests.back().log;
}

void ObjectMonitor::update(const tp::core::Time& time) {
    for (auto& test : m_tests) {
        test.updateTime = time;
        test.count = static_cast<std::size_t>(test.log.count(time));
    }
}

ConfigDB& ConfigDB::instance() {
    static ConfigDB db;
    return db;
}

void ConfigDB::reset() {
    instance().m_config = {};
}

void ConfigDB::load(const QString& module, const QString& root) {
    m_config.load(module, root);
}

Inventory& Inventory::instance() {
    static Inventory inv;
    return inv;
}

void Inventory::reset() {
    instance() = {};
}

void Inventory::setStructures(std::vector<Structure> structures) {
    m_structures = std::move(structures);
}

void Inventory::setSensors(std::vector<Sensor> sensors) {
    m_sensors = std::move(sensors);
}

void Inventory::setChannels(std::vector<Channel> channels) {
    m_channels = std::move(channels);
}

const Structure* Inventory::structure(quint32 objectId) const {
    auto it = std::find_if(m_structures.begin(), m_structures.end(),
                           [&](const Structure& s) { return s.objectId == objectId; });
    return it == m_structures.end() ? nullptr : &*it;
}

const Sensor* Inventory::sensor(quint32 objectId, quint32 sensorId) const {
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(),
                           [&](const Sensor& s) { return s.objectId == objectId && s.sensorId == sensorId; });
    return it == m_sensors.end() ? nullptr : &*it;
}

const Channel* Inventory::channel(quint32 objectId, quint32 sensorId, int component) const {
    auto it = std::find_if(m_channels.begin(), m_channels.end(),
                           [&](const Channel& c) {
                               return c.objectId == objectId && c.sensorId == sensorId && c.component == component;
                           });
    return it == m_channels.end() ? nullptr : &*it;
}

SensorLocation Inventory::location(quint32 objectId) const {
    const Structure* s = structure(objectId);
    if (!s) return {};
    return {s->lat, s->lon, 0.0};
}

bool Application::init() {
    loadConfig();
    ConfigDB::instance().load(m_settings.moduleName, m_settings.root);

    const auto queue = tp::master::queueFromName(m_settings.queue.toStdString());
    const std::string host = m_settings.masterHost.toStdString();
    m_messagingUrl = QString::fromStdString(tp::master::out(host, queue));

    for (const QString& sub : m_settings.subscriptions) addMessagingSubscription(sub.toStdString());

    if (m_settings.messagingEnabled) {
        // A subscriber is only opened when the module actually wants messages;
        // pure publishers (one-shot tools) skip it. Publisher writes to the
        // broker's input.
        if (!m_subscriptions.empty()) {
            m_subscriber = std::make_unique<tp::messaging::Subscriber>(tp::master::out(host, queue));
            for (const std::string& group : m_subscriptions) m_subscriber->subscribe(group);
        }
        m_publisher = std::make_unique<tp::messaging::Publisher>(tp::master::in(host, queue),
                                                                 /*bind=*/false,
                                                                 m_settings.storeForwardCap);
    }

    m_startMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}

void Application::publish(const tp::messaging::Message& message) {
    if (m_publisher) m_publisher->publish(message);
}

void Application::publish(const std::string& topic, const QVariantMap& header) {
    publish(tp::messaging::make(topic, header));
}

void Application::publishThenQuit(std::function<void()> emit_, int code,
                                  int settleMs, int drainMs) {
    QTimer::singleShot(settleMs, [this, emit_ = std::move(emit_), code, drainMs]() {
        emit_();
        QTimer::singleShot(drainMs, [this, code]() { exit(code); });
    });
}

void Application::pumpMessages() {
    if (!m_subscriber) return;
    // Bounded drain so a burst cannot starve the event loop.
    bool any = false;
    for (int i = 0; i < 2000; ++i) {
        auto m = m_subscriber->receive(0);
        if (!m) break;
        if (!any) { any = true; beforeBatch(); }
        ++m_messagesIn;
        if (handleRawMessage(*m)) continue;      // consumed as-is; skip decoding
        handleMessage(QString::fromStdString(m->topic),
                      tp::messaging::decodeHeader(m->header));
    }
    if (any) afterBatch();
}

void Application::sendHeartbeat() {
    if (!m_publisher) return;
    QVariantMap counters = sohCounters();
    counters["messagesIn"] = static_cast<qulonglong>(m_messagesIn);
    m_publisher->publish(tp::sohMessage(m_settings.moduleName.toStdString(), m_startMs, counters));
    handleSOH();
}

int Application::exec() {
    if (!init()) return 1;
    const bool ok = run();
    done();
    return ok ? m_returnCode : 1;
}

void Application::exit(int returnCode) {
    m_returnCode = returnCode;
    m_quit = true;
    m_notifications.close();
    m_pollTimer.stop();
    m_sohTimer.stop();
    QCoreApplication::quit();
}

void Application::addMessagingSubscription(const std::string& group) {
    m_subscriptions.insert(group);
}

void Application::sendNotification(Notification notification) {
    m_notifications.push(std::move(notification));
}

bool Application::waitEvent(Notification* out) {
    try {
        Notification n = m_notifications.pop();
        if (out) *out = std::move(n);
        return true;
    } catch (const QueueClosedException&) {
        return false;
    }
}

bool Application::run() {
    // Daemons run on the Qt event loop: one timer drains the bus, one sends the
    // heartbeat. Modules only implement handleMessage()/sohCounters().
    QObject::connect(&m_pollTimer, &QTimer::timeout, [this]() { pumpMessages(); });
    m_pollTimer.start(std::max(1, m_settings.pollIntervalMs));

    if (m_settings.sohIntervalSeconds > 0) {
        QObject::connect(&m_sohTimer, &QTimer::timeout, [this]() { sendHeartbeat(); });
        m_sohTimer.start(m_settings.sohIntervalSeconds * 1000);
    }

    // Service the store-and-forward monitor/backlog if it was enabled.
    if (m_publisher && m_settings.storeForwardCap > 0) {
        QObject::connect(&m_sfTimer, &QTimer::timeout, [this]() { m_publisher->pump(); });
        m_sfTimer.start(200);
    }

    QCoreApplication::exec();
    return true;
}

void Application::done() {}
bool Application::handleRawMessage(const tp::messaging::Message&) { return false; }
void Application::handleMessage(const QString&, const QVariantMap&) {}
void Application::handleMessage(tp::core::Message*) {}
void Application::handleTimeout() {}
void Application::handleSOH() {}
void Application::handleDisconnect() {}
void Application::handleReconnect() {}
bool Application::handleClose() { return true; }

bool StreamApplication::openStream() {
    return !settings().recordStreamUri.isEmpty();
}

void StreamApplication::closeStream() {}

bool StreamApplication::addStream(const std::string& networkCode,
                                  const std::string& stationCode,
                                  const std::string& locationCode,
                                  const std::string& channelCode) {
    m_streams.push_back(networkCode + "." + stationCode + "." + locationCode + "." + channelCode);
    return true;
}

void StreamApplication::setTimeWindow(const tp::core::TimeWindow& window) {
    m_timeWindow = window;
}

bool StreamApplication::run() {
    return Application::run();
}

} // namespace tp::client
