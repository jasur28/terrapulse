#pragma once

#include "terrapulse/geo/featureset.h"

#include <QString>

namespace tp::geo::formats {

std::size_t readGeoJSON(tp::geo::FeatureSet& featureSet, const QString& path,
                        const tp::geo::Category* category = nullptr);
bool writeGeoJSON(const QString& path, const tp::geo::Feature& feature, int indent = 2);
std::size_t writeGeoJSON(const QString& path, const tp::geo::FeatureSet& featureSet, int indent = 2);

} // namespace tp::geo::formats
