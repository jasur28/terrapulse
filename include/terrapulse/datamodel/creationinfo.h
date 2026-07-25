#pragma once

#include "terrapulse/core/datetime.h"

#include <QString>

namespace tp::datamodel {

struct CreationInfo {
    QString agencyID;
    QString author;
    tp::core::Time creationTime = tp::core::Time::now();
    tp::core::Time modificationTime = tp::core::Time::now();
};

} // namespace tp::datamodel
