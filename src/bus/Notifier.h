#pragma once
#include "bus/BusMessage.h"

#include <QString>
#include <QVariantMap>

// Data-model change notifier (like SeisComp notifiers). A change to an inventory
// object is broadcast as an INVENTORY-group message carrying an operation
// (add/update/remove) plus the object's fields, so every client can maintain its
// own copy of the data model by applying notifiers. tpmaster's dbstore persists
// the change before distributing it.
namespace tp {

enum class Op { Add, Update, Remove };

inline const char* opName(Op op) {
    switch (op) { case Op::Add: return "add"; case Op::Update: return "update";
                  default: return "remove"; }
}

// Build an `inv.<kind>.<key>` message. `fields` already carries "kind" (from the
// object's toVariant()); op is added here.
inline BusMessage notifier(Op op, const QString& kind, const QString& key,
                           const QVariantMap& fields) {
    QVariantMap h = fields;
    h["v"]    = 1;
    h["type"] = "notifier";
    h["op"]   = opName(op);
    h["kind"] = kind;

    BusMessage m;
    m.topic  = ("inv." + kind + "." + key).toStdString();
    m.header = BusMessage::encodeHeader(h);
    return m;
}

} // namespace tp
