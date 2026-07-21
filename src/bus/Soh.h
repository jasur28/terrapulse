#pragma once
#include "bus/BusMessage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QVariantMap>
#include <string>

// State-of-health (SOH) heartbeat helper. Every module periodically publishes a
// `soh.<module>` message to the STATUS group so tpmm (and later tpsoh) can show
// the health of the SYSTEM ITSELF — who is alive, uptime, throughput counters.
namespace tp {

inline BusMessage sohMessage(const std::string& module, qint64 startMs,
                             const QVariantMap& counters = {}) {
    QVariantMap h;
    h["v"]      = 1;
    h["type"]   = "soh";
    h["module"] = QString::fromStdString(module);
    h["pid"]    = static_cast<qlonglong>(QCoreApplication::applicationPid());
    h["uptime"] = static_cast<int>((QDateTime::currentMSecsSinceEpoch() - startMs) / 1000);
    for (auto it = counters.constBegin(); it != counters.constEnd(); ++it)
        h[it.key()] = it.value();

    BusMessage m;
    m.topic  = "soh." + module;
    m.header = BusMessage::encodeHeader(h);
    return m;
}

} // namespace tp
