#include "controllers/BusClient.h"
#include "bus/BusMessage.h"
#include <QDateTime>

BusClient::BusClient(const std::string& endpoint, const std::string& prefix, QObject* parent)
    : QObject(parent)
    , m_sub(endpoint)
    , m_endpoint(QString::fromStdString(endpoint))
{
    m_sub.subscribe(prefix);

    connect(&m_pollTimer, &QTimer::timeout, this, &BusClient::poll);
    m_pollTimer.start(8); // ~125 Hz; each tick drains all pending messages

    connect(&m_liveTimer, &QTimer::timeout, this, &BusClient::checkLiveness);
    m_liveTimer.start(1000);

    m_sinceLast.start();
}

void BusClient::poll() {
    bool got = false;
    QVariantMap latestSample;

    // Drain everything currently queued (non-blocking receive).
    for (int i = 0; i < 500; ++i) {
        auto m = m_sub.receive(0);
        if (!m) break;
        got = true;

        const QVariantMap h = tp::BusMessage::decodeHeader(m->header);
        m_station    = h.value("station").toUInt();
        m_object     = h.value("object").toUInt();
        m_sensor     = h.value("sensor").toUInt();
        m_sampleRate = h.value("sampleRate").toDouble();
        m_lastX      = h.value("x").toDouble();
        m_lastY      = h.value("y").toDouble();
        m_lastZ      = h.value("z").toDouble();
        const qint64 t = h.value("t").toLongLong();
        m_lastTimestamp = QDateTime::fromMSecsSinceEpoch(t).toString("yyyy-MM-dd HH:mm:ss.zzz");
        ++m_packetCount;

        QVariantMap sample;
        sample["timestampMs"] = t;
        sample["sampleRate"]  = m_sampleRate > 0.0 ? m_sampleRate : 100;
        sample["x"]           = m_lastX;
        sample["y"]           = m_lastY;
        sample["z"]           = m_lastZ;
        sample["sequence"]    = h.value("seq");
        latestSample = sample;
    }

    if (got) {
        m_sinceLast.restart();
        if (!m_connected) {
            m_connected = true;
            emit connectedChanged();
        }
        // QML gets the newest sample per poll tick. Raw archival/review paths
        // keep full-rate data; the live GUI must stay responsive under load.
        if (!latestSample.isEmpty())
            emit sampleReceived(latestSample);
        // Emit aggregate UI notifications once per tick, not per sample.
        emit sampleChanged();
        emit statsChanged();
    }
}

void BusClient::checkLiveness() {
    if (m_connected && m_sinceLast.elapsed() > 2000) {
        m_connected = false;
        emit connectedChanged();
    }
}
