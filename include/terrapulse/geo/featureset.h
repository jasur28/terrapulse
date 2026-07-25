#pragma once

#include "terrapulse/core/baseobject.h"
#include "terrapulse/geo/feature.h"

#include <memory>
#include <vector>

namespace tp::geo {

class FeatureSet;

class FeatureSetObserver {
public:
    virtual ~FeatureSetObserver() = default;
    virtual void featureSetUpdated() = 0;
};

class FeatureSet : public tp::core::BaseObject {
public:
    using Features = std::vector<FeaturePtr>;
    using Categories = std::vector<std::shared_ptr<Category>>;

    const char* className() const override { return "Geo::FeatureSet"; }

    void clear();
    bool addFeature(FeaturePtr feature);
    std::shared_ptr<Category> addCategory(QString name, const Category* parent = nullptr);

    bool registerObserver(FeatureSetObserver* observer);
    bool unregisterObserver(FeatureSetObserver* observer);
    void notifyUpdated();

    const Features& features() const { return m_features; }
    const Categories& categories() const { return m_categories; }
    BoundingBox bbox() const;

private:
    Features m_features;
    Categories m_categories;
    std::vector<FeatureSetObserver*> m_observers;
};

FeatureSet& defaultFeatureSet();

} // namespace tp::geo
