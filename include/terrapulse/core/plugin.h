#pragma once

#include <string>

namespace tp::core {

struct PluginDescriptor {
    std::string name;
    std::string type;
    std::string module;
    std::string description;
};

} // namespace tp::core
