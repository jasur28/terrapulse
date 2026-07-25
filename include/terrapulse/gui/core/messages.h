#pragma once

#include "terrapulse/core/message.h"
#include "terrapulse/gui/qt.h"

#include <QString>

namespace tp::gui {

enum class Command {
    None,
    ShowSettings,
    ShowInventory,
    ShowLog,
    CenterMap,
    SelectObject,
    StartModule,
    StopModule
};

enum class NotificationLevel {
    Information,
    Warning,
    Error
};

struct MessageGroups {
    QString inventory = "INVENTORY";
    QString qualityControl = "QC";
    QString event = "EVENT";
    QString waveform = "WAVEFORM";
    QString journal = "JOURNAL";
};

class TP_GUI_API CommandMessage : public tp::core::Message {
public:
    Command command = Command::None;
    QString parameter;
};

} // namespace tp::gui
