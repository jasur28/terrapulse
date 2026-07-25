#pragma once

#include <string>
#include <typeindex>

namespace tp::core {

struct TypeInfo {
    std::string className;
    std::type_index type = typeid(void);
};

} // namespace tp::core
