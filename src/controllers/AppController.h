#pragma once
#include "bus/Bus.h"
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <string>

// AppController — bus relay for the UI. Subscribes to the analysis results and
// re-emits them to QML. No analysis here (that lives in the daemons); this only
// forwards decoded maps:
//   saf. features   shf. anomalies   evt. grouped events
//   wfp. strong motion (PGA/PGV/response spectrum/JMA)   qc. data quality
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowsProcessed READ windowsProcessed NOTIFY windowsProcessedChanged)

public:
    explicit AppController(const std::string& endpoint, QObject* parent = nullptr);

    int windowsProcessed() const { return m_windowsProcessed; }

signals:
    void safReceived(const QVariantMap& saf);
    void shfReceived(const QVariantMap& shf);
    void eventReceived(const QVariantMap& evt);      // tpevent: structure-level event
    void wfparamReceived(const QVariantMap& wfp);    // tpwfparam: PGA/PGV/PSA/JMA
    void qcReceived(const QVariantMap& qc);          // tpqc: data-quality verdict
    void windowsProcessedChanged();

private slots:
    void poll();

private:
    tp::Subscriber m_sub;
    QTimer         m_timer;
    int            m_windowsProcessed = 0;
};
