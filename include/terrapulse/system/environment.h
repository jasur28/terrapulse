#pragma once

#include "config/Config.h"

#include <QString>

namespace tp::system {

inline QString installRoot(const QString& hint = QString()) {
    return Config::discoverRoot(hint);
}

} // namespace tp::system
