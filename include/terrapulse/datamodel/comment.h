#pragma once

#include "terrapulse/datamodel/creationinfo.h"

#include <QString>

namespace tp::datamodel {

struct Comment {
    QString id;
    QString text;
    CreationInfo creationInfo;
};

} // namespace tp::datamodel
