#pragma once
#include <string>
#include <QByteArray>
#include <QVariantMap>

namespace tp {

// One bus message = [topic][header][payload].
//   topic   — ASCII routing prefix, e.g. "saf.1.1.1.2"
//   header  — CBOR-encoded routing metadata (v, type, station, object, sensor, axis, seq, t_*)
//   payload — binary: serialized SDF/SAF/SHF bytes (already CRC'd)
struct BusMessage {
    std::string topic;
    QByteArray  header;
    QByteArray  payload;

    static QByteArray  encodeHeader(const QVariantMap& fields);
    static QVariantMap decodeHeader(const QByteArray& bytes);
};

} // namespace tp
