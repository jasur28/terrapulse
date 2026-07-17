#include "bus/BusMessage.h"
#include <QCborValue>

namespace tp {

QByteArray BusMessage::encodeHeader(const QVariantMap& fields) {
    return QCborValue::fromVariant(fields).toCbor();
}

QVariantMap BusMessage::decodeHeader(const QByteArray& bytes) {
    return QCborValue::fromCbor(bytes).toVariant().toMap();
}

} // namespace tp
