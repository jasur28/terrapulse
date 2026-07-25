#pragma once

#include <QHostAddress>
#include <QString>

namespace tp::io {

struct SocketAddress {
    QString host = "127.0.0.1";
    quint16 port = 0;

    QString toString() const { return QString("%1:%2").arg(host).arg(port); }
};

} // namespace tp::io
