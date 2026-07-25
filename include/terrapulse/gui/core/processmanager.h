#pragma once

#include "terrapulse/gui/qt.h"

#include <QString>

#include <vector>

namespace tp::gui {

struct ModuleProcess {
    QString name;
    QString command;
    bool running = false;
    int pid = 0;
};

class TP_GUI_API ProcessManager {
public:
    void setModules(std::vector<ModuleProcess> modules) { m_modules = std::move(modules); }
    const std::vector<ModuleProcess>& modules() const { return m_modules; }
    ModuleProcess* module(const QString& name);

private:
    std::vector<ModuleProcess> m_modules;
};

} // namespace tp::gui
