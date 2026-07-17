#include "controllers/AppController.h"
#include "bus/BusMessage.h"

AppController::AppController(const std::string& endpoint, QObject* parent)
    : QObject(parent)
    , m_sub(endpoint)
{
    m_sub.subscribe("saf.");
    m_sub.subscribe("shf.");

    connect(&m_timer, &QTimer::timeout, this, &AppController::poll);
    m_timer.start(16); // ~60 Hz; drains all pending results each tick
}

void AppController::poll() {
    bool windowsChanged = false;

    for (int i = 0; i < 4000; ++i) {
        auto m = m_sub.receive(0);
        if (!m) break;

        const QVariantMap map = tp::BusMessage::decodeHeader(m->header);

        if (m->topic.rfind("saf.", 0) == 0) {
            emit safReceived(map);
            // One window == one Z-axis SAF; count those for the dashboard tile.
            if (map.value("component").toInt() == 2) {
                ++m_windowsProcessed;
                windowsChanged = true;
            }
        } else if (m->topic.rfind("shf.", 0) == 0) {
            emit shfReceived(map);
        }
    }

    if (windowsChanged)
        emit windowsProcessedChanged();
}
