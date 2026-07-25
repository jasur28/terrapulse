#pragma once

#include "terrapulse/geo/featureset.h"

#include <functional>
#include <vector>

namespace tp::geo::index {

class QuadTree {
public:
    using VisitFunc = std::function<bool(const tp::geo::Feature*)>;

    void clear();
    void addItem(const tp::geo::Feature* feature);
    void add(const tp::geo::FeatureSet& featureSet);

    const tp::geo::BoundingBox& bbox() const { return m_bbox; }

    void query(const tp::geo::Coordinate& point, const VisitFunc& visitor) const;
    void query(const tp::geo::BoundingBox& area, const VisitFunc& visitor, bool clipOnlyNodes = false) const;

    const tp::geo::Feature* findFirst(const tp::geo::Coordinate& point) const;
    const tp::geo::Feature* findLast(const tp::geo::Coordinate& point) const;

private:
    std::vector<const tp::geo::Feature*> m_features;
    tp::geo::BoundingBox m_bbox;
};

} // namespace tp::geo::index
