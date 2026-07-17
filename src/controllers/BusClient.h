#pragma once
#include "bus/Bus.h"
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantMap>
#include <QString>
#include <string>

// BusClient — subscribes to the ZeroMQ bus and re-emits raw samples on the Qt
// main thread, mimicking the old SerialStreamReceiver::sampleReceived interface
// so the UI and AppController need no behavioural change. Read-only: tpacq owns
// the serial port; this just consumes `raw.*` messages.
class BusClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool       connected      READ connected      NOTIFY connectedChanged)
    Q_PROPERTY(quint32    station        READ station        NOTIFY sampleChanged)
    Q_PROPERTY(quint32    object         READ object         NOTIFY sampleChanged)
    Q_PROPERTY(quint32    sensor         READ sensor         NOTIFY sampleChanged)
    Q_PROPERTY(double     sampleRate     READ sampleRate     NOTIFY sampleChanged)
    Q_PROPERTY(qulonglong packetCount    READ packetCount    NOTIFY statsChanged)
    Q_PROPERTY(QString    lastTimestamp  READ lastTimestamp  NOTIFY sampleChanged)
    Q_PROPERTY(double     lastX          READ lastX          NOTIFY sampleChanged)
    Q_PROPERTY(double     lastY          READ lastY          NOTIFY sampleChanged)
    Q_PROPERTY(double     lastZ          READ lastZ          NOTIFY sampleChanged)
    Q_PROPERTY(QString    endpoint       READ endpoint       CONSTANT)

public:
    explicit BusClient(const std::string& endpoint, const std::string& prefix,
                       QObject* parent = nullptr);

    bool       connected()     const { return m_connected; }
    quint32    station()       const { return m_station; }
    quint32    object()        const { return m_object; }
    quint32    sensor()        const { return m_sensor; }
    double     sampleRate()    const { return m_sampleRate; }
    qulonglong packetCount()   const { return m_packetCount; }
    QString    lastTimestamp() const { return m_lastTimestamp; }
    double     lastX()         const { return m_lastX; }
    double     lastY()         const { return m_lastY; }
    double     lastZ()         const { return m_lastZ; }
    QString    endpoint()      const { return m_endpoint; }

signals:
    void sampleReceived(const QVariantMap& sample);
    void connectedChanged();
    void sampleChanged();
    void statsChanged();

private slots:
    void poll();          // drain pending bus messages
    void checkLiveness(); // flip connected=false if the stream goes quiet

private:
    tp::Subscriber m_sub;
    QString        m_endpoint;
    QTimer         m_pollTimer;
    QTimer         m_liveTimer;
    QElapsedTimer  m_sinceLast;

    bool       m_connected   = false;
    quint32    m_station     = 0;
    quint32    m_object      = 0;
    quint32    m_sensor      = 0;
    double     m_sampleRate  = 0.0;
    qulonglong m_packetCount = 0;
    QString    m_lastTimestamp;
    double     m_lastX = 0.0;
    double     m_lastY = 0.0;
    double     m_lastZ = 0.0;
};
