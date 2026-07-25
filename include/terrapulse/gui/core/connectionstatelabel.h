#pragma once

#include "terrapulse/gui/qt.h"

#include <QColor>
#include <QString>

namespace tp::gui {

enum class ConnectionState {
    Offline,
    Connecting,
    Connected,
    Error
};

struct TP_GUI_API ConnectionStateLabel {
    ConnectionState state = ConnectionState::Offline;
    QString text = "Offline";
    QColor color{110, 113, 121};

    void setState(ConnectionState value);
};

} // namespace tp::gui
