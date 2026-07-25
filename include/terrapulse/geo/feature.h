#pragma once

#include "terrapulse/core/baseobject.h"
#include "terrapulse/geo/boundingbox.h"

#include <QHash>
#include <QString>

#include <memory>
#include <vector>

namespace tp::geo {

struct Category {
    unsigned int id = 0;
    QString name;
    QString localName;
    const Category* parent = nullptr;
    QString dataDir;
};

class Feature : public tp::core::BaseObject {
public:
    using Coordinates = std::vector<Coordinate>;
    using SubFeatures = std::vector<std::size_t>;
    using Attributes = QHash<QString, QString>;

    Feature() = default;
    explicit Feature(QString name, const Category* category = nullptr, unsigned int rank = 1);

    const char* className() const override { return "Geo::Feature"; }

    QString name() const { return m_name; }
    void setName(QString name) { m_name = std::move(name); }

    const Category* category() const { return m_category; }
    void setCategory(const Category* category) { m_category = category; }

    unsigned int rank() const { return m_rank; }
    void setRank(unsigned int rank) { m_rank = rank; }

    void setAttribute(const QString& name, const QString& value);
    QString attribute(const QString& name) const;
    const Attributes& attributes() const { return m_attributes; }

    void addVertex(const Coordinate& vertex, bool newSubFeature = false);
    void addVertex(double latitude, double longitude, bool newSubFeature = false);

    bool closedPolygon() const { return m_closedPolygon; }
    void setClosedPolygon(bool closed) { m_closedPolygon = closed; }

    const Coordinates& vertices() const { return m_vertices; }
    const SubFeatures& subFeatures() const { return m_subFeatures; }
    const BoundingBox& bbox() const { return m_bbox; }

    void updateBoundingBox();
    void invertOrder();
    bool contains(const Coordinate& value) const;
    double area() const;

    void setUserData(void* data) { m_userData = data; }
    void* userData() const { return m_userData; }

private:
    QString m_name;
    const Category* m_category = nullptr;
    void* m_userData = nullptr;
    unsigned int m_rank = 1;
    Attributes m_attributes;
    Coordinates m_vertices;
    SubFeatures m_subFeatures;
    bool m_closedPolygon = false;
    BoundingBox m_bbox;
};

using FeaturePtr = std::shared_ptr<Feature>;

} // namespace tp::geo
