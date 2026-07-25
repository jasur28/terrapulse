#pragma once

#include "terrapulse/gui/core/maps.h"
#include "terrapulse/gui/core/messages.h"
#include "terrapulse/gui/qt.h"
#include "terrapulse/gui/core/scheme.h"

#include <QString>

namespace tp::gui {

struct ApplicationShell {
    const char* qmlType = "TpGuiApplication";
};

class TP_GUI_API ApplicationContext {
public:
    enum Flag {
        ShowSplash = 0x001,
        WantDatabase = 0x002,
        WantMessaging = 0x004,
        AutoApplyNotifier = 0x008,
        LoadInventory = 0x010,
        Default = ShowSplash | WantDatabase | WantMessaging | AutoApplyNotifier | LoadInventory
    };

    explicit ApplicationContext(QString moduleName = {}, int flags = Default);

    QString moduleName() const { return m_moduleName; }
    int flags() const { return m_flags; }
    Scheme& scheme() { return m_scheme; }
    const Scheme& scheme() const { return m_scheme; }
    MapsDesc& maps() { return m_maps; }
    const MapsDesc& maps() const { return m_maps; }
    MessageGroups& messageGroups() { return m_messageGroups; }
    const MessageGroups& messageGroups() const { return m_messageGroups; }

private:
    QString m_moduleName;
    int m_flags = Default;
    Scheme m_scheme;
    MapsDesc m_maps;
    MessageGroups m_messageGroups;
};

} // namespace tp::gui
