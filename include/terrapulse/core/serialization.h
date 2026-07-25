#pragma once

#include "terrapulse/core/archive.h"

namespace tp::core {

class Serializable {
public:
    virtual ~Serializable() = default;
    virtual void serialize(Archive& archive) = 0;
};

} // namespace tp::core
