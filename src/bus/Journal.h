#pragma once
#include "bus/BusMessage.h"

#include <QDateTime>
#include <QString>
#include <QVariantMap>

// Operator journal command (like SeisComp's journaling): an operator's action on
// an anomaly event — confirm / reject / reclassify / comment. Journals are the
// audit trail (explainability): who did what, when. Distributed on the JOURNAL
// group and persisted by tpmaster's dbstore, which also updates the event's
// review status.
namespace tp {

inline BusMessage journalMessage(qulonglong eventId, const QString& action,
                                 const QString& op, const QString& note = QString()) {
    QVariantMap h;
    h["v"]        = 1;
    h["type"]     = "journal";
    h["eventId"]  = eventId;          // target SHF id
    h["action"]   = action;           // "confirm" | "reject" | "reclassify" | "comment"
    h["operator"] = op;
    h["note"]     = note;
    h["t"]        = static_cast<qlonglong>(QDateTime::currentMSecsSinceEpoch());

    BusMessage m;
    m.topic  = "jrnl." + std::to_string(eventId);
    m.header = BusMessage::encodeHeader(h);
    return m;
}

} // namespace tp
