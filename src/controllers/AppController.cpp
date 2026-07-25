#include "controllers/AppController.h"
#include "bus/BusMessage.h"

AppController::AppController(const std::string& endpoint, QObject* parent)
    : QObject(parent)
    , m_sub(endpoint)
{
    m_sub.subscribe("saf.");
    m_sub.subscribe("shf.");
    m_sub.subscribe("evt.");
    m_sub.subscribe("wfp.");
    m_sub.subscribe("qc.");

    connect(&m_timer, &QTimer::timeout, this, &AppController::poll);
    m_timer.start(50); // ~20 Hz; bounded drain keeps the GUI event loop responsive
}

void AppController::poll() {
    bool windowsChanged = false;

    for (int i = 0; i < 500; ++i) {
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
        } else if (m->topic.rfind("evt.", 0) == 0) {
            emit eventReceived(map);
        } else if (m->topic.rfind("wfp.", 0) == 0) {
            emit wfparamReceived(map);
        } else if (m->topic.rfind("qc.", 0) == 0) {
            emit qcReceived(map);
        }
    }

    if (windowsChanged)
        emit windowsProcessedChanged();
}
