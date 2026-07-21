#pragma once
#include "bus/Bus.h"
#include "bus/Master.h"

#include <QObject>
#include <QTimer>
#include <QString>
#include <string>

// JournalController — the UI's operator-action channel. Publishes journal
// commands (confirm/reject/reclassify/comment) to the master and listens on the
// JOURNAL group so every console reflects the same review status (audit trail).
class JournalController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString operatorName READ operatorName WRITE setOperatorName NOTIFY operatorNameChanged)

public:
    explicit JournalController(const std::string& host,
                               tp::master::Queue queue = tp::master::Queue::Production,
                               QObject* parent = nullptr);

    QString operatorName() const { return m_operator; }
    void setOperatorName(const QString& n) {
        if (m_operator != n) { m_operator = n; emit operatorNameChanged(); }
    }

    Q_INVOKABLE void sendAction(qulonglong eventId, const QString& action,
                                const QString& note = QString());

signals:
    void journalReceived(qulonglong eventId, const QString& action, const QString& op);
    void operatorNameChanged();

private slots:
    void poll();

private:
    tp::Publisher  m_pub;    // -> master IN
    tp::Subscriber m_sub;    // <- master OUT (jrnl.)
    QTimer         m_timer;
    QString        m_operator = "operator";
};
