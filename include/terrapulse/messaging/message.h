#pragma once

#include "bus/BusMessage.h"

#include <QString>
#include <QVariantMap>

#include <string>

namespace tp::messaging {

using Message = tp::BusMessage;

// Header codec, so a module never has to reach into the bus implementation for
// the wire format.
inline QByteArray encodeHeader(const QVariantMap& header) {
    return tp::BusMessage::encodeHeader(header);
}
inline QVariantMap decodeHeader(const QByteArray& raw) {
    return tp::BusMessage::decodeHeader(raw);
}

// Build a ready-to-publish message. Keeping this here means the topic and the
// header are always encoded the same way across modules.
inline Message make(const std::string& topic, const QVariantMap& header,
                    const QByteArray& payload = {}) {
    Message m;
    m.topic   = topic;
    m.header  = encodeHeader(header);
    m.payload = payload;
    return m;
}

// Same, with the routing ids appended in the canonical order used on the bus:
//   <group>.<station>.<object>.<sensor>[.<axis>]
inline Message make(const QString& group, quint32 station, quint32 object, quint32 sensor,
                    const QVariantMap& header, int axis = -1) {
    std::string topic = group.toStdString() + "." + std::to_string(station) + "."
                      + std::to_string(object) + "." + std::to_string(sensor);
    if (axis >= 0) topic += "." + std::to_string(axis);
    return make(topic, header);
}

} // namespace tp::messaging
