#pragma once

#include <QString>

namespace tp::datamodel::strongmotion {

struct FilterParameter {
    QString type;
    double lowFrequency = 0.0;
    double highFrequency = 0.0;
    int order = 0;
};

} // namespace tp::datamodel::strongmotion
