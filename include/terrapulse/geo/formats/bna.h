#pragma once

#include "terrapulse/geo/featureset.h"

#include <QString>

namespace tp::geo::formats {

std::size_t readBNA(tp::geo::FeatureSet& featureSet, const QString& path,
                    const tp::geo::Category* category = nullptr);

} // namespace tp::geo::formats
