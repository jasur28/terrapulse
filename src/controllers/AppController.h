#pragma once
#include "bus/Bus.h"
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <string>

// AppController — bus relay for the UI. Subscribes to SAF/SHF results published
// by the tpproc daemon and re-emits them to QML. No analysis here anymore (that
// moved to tpproc); this just forwards decoded result maps.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowsProcessed READ windowsProcessed NOTIFY windowsProcessedChanged)

public:
    explicit AppController(const std::string& endpoint, QObject* parent = nullptr);

    int windowsProcessed() const { return m_windowsProcessed; }

signals:
    void safReceived(const QVariantMap& saf);
    void shfReceived(const QVariantMap& shf);
    void windowsProcessedChanged();

private slots:
    void poll();

private:
    tp::Subscriber m_sub;
    QTimer         m_timer;
    int            m_windowsProcessed = 0;
};
