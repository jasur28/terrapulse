#pragma once

#include "terrapulse/datamodel/publicobject.h"

namespace tp::gui::datamodel {

class PublicObjectEvaluator {
public:
    virtual ~PublicObjectEvaluator() = default;
    virtual int score(const tp::datamodel::PublicObject& object) const = 0;
};

} // namespace tp::gui::datamodel
