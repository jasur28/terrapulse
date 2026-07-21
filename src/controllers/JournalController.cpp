#include "controllers/JournalController.h"
#include "bus/Journal.h"
#include "bus/BusMessage.h"
#include "bus/Master.h"

JournalController::JournalController(const std::string& host, tp::master::Queue queue, QObject* parent)
    : QObject(parent)
    , m_pub(tp::master::in(host, queue), /*bind=*/false)
    , m_sub(tp::master::out(host, queue))
{
    m_sub.subscribe("jrnl.");
    connect(&m_timer, &QTimer::timeout, this, &JournalController::poll);
    m_timer.start(200);
}

void JournalController::sendAction(qulonglong eventId, const QString& action, const QString& note) {
    m_pub.publish(tp::journalMessage(eventId, action, m_operator, note));
}

void JournalController::poll() {
    for (int i = 0; i < 1000; ++i) {
        auto m = m_sub.receive(0);
        if (!m) break;
        const QVariantMap h = tp::BusMessage::decodeHeader(m->header);
        emit journalReceived(h.value("eventId").toULongLong(),
                             h.value("action").toString(),
                             h.value("operator").toString());
    }
}
