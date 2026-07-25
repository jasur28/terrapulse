#include "terrapulse/geo.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tp::geo {
namespace {

constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kPi = 3.14159265358979323846;

double degToRad(double value) {
    return value * kPi / 180.0;
}

bool lonBetween(double lon, double west, double east) {
    lon = Coordinate::normalizeLon(lon);
    west = Coordinate::normalizeLon(west);
    east = Coordinate::normalizeLon(east);
    if (west <= east) {
        return lon >= west && lon <= east;
    }
    return lon >= west || lon <= east;
}

Coordinate jsonCoordinate(const QJsonArray& value) {
    if (value.size() < 2) {
        return {};
    }
    return Coordinate(value.at(1).toDouble(), value.at(0).toDouble()).normalize();
}

void readLineString(Feature& feature, const QJsonArray& coordinates, bool polygon) {
    bool first = true;
    for (const auto& point : coordinates) {
        feature.addVertex(jsonCoordinate(point.toArray()), !first && false);
        first = false;
    }
    feature.setClosedPolygon(polygon);
}

void readPolygon(Feature& feature, const QJsonArray& rings) {
    bool firstRing = true;
    for (const auto& ringValue : rings) {
        const auto ring = ringValue.toArray();
        bool firstPoint = true;
        for (const auto& point : ring) {
            feature.addVertex(jsonCoordinate(point.toArray()), !firstRing && firstPoint);
            firstPoint = false;
        }
        firstRing = false;
    }
    feature.setClosedPolygon(true);
}

void readGeometry(Feature& feature, const QJsonObject& geometry) {
    const auto type = geometry.value("type").toString();
    const auto coordinates = geometry.value("coordinates").toArray();

    if (type == "Point") {
        feature.addVertex(jsonCoordinate(coordinates));
    }
    else if (type == "LineString") {
        readLineString(feature, coordinates, false);
    }
    else if (type == "Polygon") {
        readPolygon(feature, coordinates);
    }
    else if (type == "MultiPolygon") {
        bool first = true;
        for (const auto& polygonValue : coordinates) {
            auto polygon = polygonValue.toArray();
            if (!first && !polygon.isEmpty()) {
                auto ring = polygon.first().toArray();
                if (!ring.isEmpty()) {
                    feature.addVertex(jsonCoordinate(ring.first().toArray()), true);
                    ring.removeFirst();
                    for (const auto& point : ring) {
                        feature.addVertex(jsonCoordinate(point.toArray()));
                    }
                    polygon.removeFirst();
                    for (const auto& hole : polygon) {
                        for (const auto& point : hole.toArray()) {
                            feature.addVertex(jsonCoordinate(point.toArray()), true);
                        }
                    }
                }
            }
            else {
                readPolygon(feature, polygon);
            }
            first = false;
        }
        feature.setClosedPolygon(true);
    }
}

QJsonArray writeCoordinates(const Feature& feature) {
    QJsonArray ring;
    for (const auto& vertex : feature.vertices()) {
        QJsonArray point;
        point.append(vertex.longitude());
        point.append(vertex.latitude());
        ring.append(point);
    }
    return ring;
}

} // namespace

Coordinate& Coordinate::normalize() {
    normalizeLatLon(lat, lon);
    return *this;
}

bool Coordinate::isValid() const {
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

QString Coordinate::toString() const {
    return QString("%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);
}

Coordinate::ValueType Coordinate::normalizeLat(ValueType latitude) {
    return std::clamp(latitude, -90.0, 90.0);
}

Coordinate::ValueType Coordinate::normalizeLon(ValueType longitude) {
    while (longitude < -180.0) {
        longitude += 360.0;
    }
    while (longitude > 180.0) {
        longitude -= 360.0;
    }
    return longitude;
}

void Coordinate::normalizeLatLon(ValueType& latitude, ValueType& longitude) {
    latitude = normalizeLat(latitude);
    longitude = normalizeLon(longitude);
}

Coordinate::ValueType Coordinate::longitudeWidth(ValueType west, ValueType east) {
    west = normalizeLon(west);
    east = normalizeLon(east);
    return west <= east ? east - west : 360.0 - west + east;
}

Coordinate::ValueType Coordinate::distanceLongitude(ValueType lon0, ValueType lon1) {
    auto diff = std::abs(normalizeLon(lon1) - normalizeLon(lon0));
    return diff > 180.0 ? 360.0 - diff : diff;
}

double Coordinate::distanceKm(const Coordinate& a, const Coordinate& b) {
    const double dLat = degToRad(b.lat - a.lat);
    const double dLon = degToRad(b.lon - a.lon);
    const double lat1 = degToRad(a.lat);
    const double lat2 = degToRad(b.lat);
    const double h = std::sin(dLat / 2.0) * std::sin(dLat / 2.0)
        + std::cos(lat1) * std::cos(lat2) * std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    return 2.0 * kEarthRadiusKm * std::asin(std::min(1.0, std::sqrt(h)));
}

bool contains(const Coordinate& point, const Coordinate* polygon, std::size_t sides) {
    if (!polygon || sides < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = sides - 1; i < sides; j = i++) {
        const auto& a = polygon[i];
        const auto& b = polygon[j];
        const bool intersects = ((a.lat > point.lat) != (b.lat > point.lat))
            && (point.lon < (b.lon - a.lon) * (point.lat - a.lat) / (b.lat - a.lat + 1e-12) + a.lon);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

double area(const Coordinate* polygon, std::size_t sides) {
    if (!polygon || sides < 3) {
        return 0.0;
    }

    double sum = 0.0;
    for (std::size_t i = 0, j = sides - 1; i < sides; j = i++) {
        sum += (polygon[j].lon + polygon[i].lon) * (polygon[j].lat - polygon[i].lat);
    }
    return std::abs(sum) * 0.5;
}

BoundingBox::BoundingBox() = default;

BoundingBox::BoundingBox(ValueType s, ValueType w, ValueType n, ValueType e)
    : north(n), south(s), east(e), west(w), m_empty(false) {
    normalize();
}

bool BoundingBox::isNull() const {
    return !m_empty && north == 0.0 && south == 0.0 && east == 0.0 && west == 0.0;
}

void BoundingBox::reset() {
    north = south = east = west = 0.0;
    m_empty = true;
}

BoundingBox& BoundingBox::normalize() {
    if (m_empty) {
        return *this;
    }
    south = Coordinate::normalizeLat(south);
    north = Coordinate::normalizeLat(north);
    if (south > north) {
        std::swap(south, north);
    }
    west = Coordinate::normalizeLon(west);
    east = Coordinate::normalizeLon(east);
    return *this;
}

bool BoundingBox::crossesDateLine() const {
    return !m_empty && west > east;
}

bool BoundingBox::coversFullLongitude() const {
    return !m_empty && width() >= 360.0;
}

BoundingBox::ValueType BoundingBox::width() const {
    return m_empty ? 0.0 : Coordinate::longitudeWidth(west, east);
}

BoundingBox::ValueType BoundingBox::height() const {
    return m_empty ? 0.0 : north - south;
}

Coordinate BoundingBox::center() const {
    if (m_empty) {
        return {};
    }
    auto cLon = west + width() / 2.0;
    return Coordinate((south + north) / 2.0, Coordinate::normalizeLon(cLon));
}

bool BoundingBox::contains(const Coordinate& value) const {
    return !m_empty && value.lat >= south && value.lat <= north && lonBetween(value.lon, west, east);
}

bool BoundingBox::contains(const BoundingBox& other) const {
    if (m_empty || other.m_empty) {
        return false;
    }
    return contains({other.south, other.west}) && contains({other.north, other.east});
}

bool BoundingBox::intersects(const BoundingBox& other) const {
    if (m_empty || other.m_empty || north < other.south || south > other.north) {
        return false;
    }
    return contains({other.south, other.west}) || contains({other.north, other.east})
        || other.contains({south, west}) || other.contains({north, east});
}

BoundingBox::Relation BoundingBox::relation(const BoundingBox& other) const {
    if (contains(other)) {
        return Relation::Contains;
    }
    return intersects(other) ? Relation::Intersects : Relation::Disjoint;
}

void BoundingBox::merge(const BoundingBox& other) {
    if (other.m_empty) {
        return;
    }
    if (m_empty) {
        *this = other;
        return;
    }
    south = std::min(south, other.south);
    north = std::max(north, other.north);
    west = std::min(west, other.west);
    east = std::max(east, other.east);
    normalize();
}

void BoundingBox::expand(const Coordinate& value) {
    auto normalized = value;
    normalized.normalize();
    if (m_empty) {
        south = north = normalized.lat;
        west = east = normalized.lon;
        m_empty = false;
        return;
    }
    south = std::min(south, normalized.lat);
    north = std::max(north, normalized.lat);
    west = std::min(west, normalized.lon);
    east = std::max(east, normalized.lon);
}

void BoundingBox::fromPolygon(const Coordinate* coords, std::size_t count) {
    reset();
    for (std::size_t i = 0; i < count; ++i) {
        expand(coords[i]);
    }
}

Feature::Feature(QString name, const Category* category, unsigned int rank)
    : m_name(std::move(name)), m_category(category), m_rank(rank) {}

void Feature::setAttribute(const QString& name, const QString& value) {
    m_attributes.insert(name, value);
}

QString Feature::attribute(const QString& name) const {
    return m_attributes.value(name);
}

void Feature::addVertex(const Coordinate& vertex, bool newSubFeature) {
    if (newSubFeature || m_vertices.empty()) {
        m_subFeatures.push_back(m_vertices.size());
    }
    auto normalized = vertex;
    normalized.normalize();
    m_vertices.push_back(normalized);
    m_bbox.expand(normalized);
}

void Feature::addVertex(double latitude, double longitude, bool newSubFeature) {
    addVertex(Coordinate(latitude, longitude), newSubFeature);
}

void Feature::updateBoundingBox() {
    m_bbox.fromPolygon(m_vertices.data(), m_vertices.size());
}

void Feature::invertOrder() {
    std::reverse(m_vertices.begin(), m_vertices.end());
}

bool Feature::contains(const Coordinate& value) const {
    return m_bbox.contains(value) && (!m_closedPolygon || tp::geo::contains(value, m_vertices.data(), m_vertices.size()));
}

double Feature::area() const {
    return tp::geo::area(m_vertices.data(), m_vertices.size());
}

void FeatureSet::clear() {
    m_features.clear();
    m_categories.clear();
    notifyUpdated();
}

bool FeatureSet::addFeature(FeaturePtr feature) {
    if (!feature) {
        return false;
    }
    m_features.push_back(std::move(feature));
    notifyUpdated();
    return true;
}

std::shared_ptr<Category> FeatureSet::addCategory(QString name, const Category* parent) {
    auto category = std::make_shared<Category>();
    category->id = static_cast<unsigned int>(m_categories.size() + 1);
    category->name = std::move(name);
    category->parent = parent;
    m_categories.push_back(category);
    return category;
}

bool FeatureSet::registerObserver(FeatureSetObserver* observer) {
    if (!observer || std::find(m_observers.begin(), m_observers.end(), observer) != m_observers.end()) {
        return false;
    }
    m_observers.push_back(observer);
    return true;
}

bool FeatureSet::unregisterObserver(FeatureSetObserver* observer) {
    auto it = std::remove(m_observers.begin(), m_observers.end(), observer);
    if (it == m_observers.end()) {
        return false;
    }
    m_observers.erase(it, m_observers.end());
    return true;
}

void FeatureSet::notifyUpdated() {
    for (auto* observer : m_observers) {
        observer->featureSetUpdated();
    }
}

BoundingBox FeatureSet::bbox() const {
    BoundingBox result;
    for (const auto& feature : m_features) {
        if (feature) {
            result.merge(feature->bbox());
        }
    }
    return result;
}

FeatureSet& defaultFeatureSet() {
    static FeatureSet set;
    return set;
}

namespace index {

void QuadTree::clear() {
    m_features.clear();
    m_bbox.reset();
}

void QuadTree::addItem(const tp::geo::Feature* feature) {
    if (!feature) {
        return;
    }
    m_features.push_back(feature);
    m_bbox.merge(feature->bbox());
}

void QuadTree::add(const tp::geo::FeatureSet& featureSet) {
    for (const auto& feature : featureSet.features()) {
        addItem(feature.get());
    }
}

void QuadTree::query(const tp::geo::Coordinate& point, const VisitFunc& visitor) const {
    for (const auto* feature : m_features) {
        if (feature && feature->contains(point) && !visitor(feature)) {
            return;
        }
    }
}

void QuadTree::query(const tp::geo::BoundingBox& area, const VisitFunc& visitor, bool) const {
    for (const auto* feature : m_features) {
        if (feature && feature->bbox().intersects(area) && !visitor(feature)) {
            return;
        }
    }
}

const tp::geo::Feature* QuadTree::findFirst(const tp::geo::Coordinate& point) const {
    for (const auto* feature : m_features) {
        if (feature && feature->contains(point)) {
            return feature;
        }
    }
    return nullptr;
}

const tp::geo::Feature* QuadTree::findLast(const tp::geo::Coordinate& point) const {
    const tp::geo::Feature* result = nullptr;
    for (const auto* feature : m_features) {
        if (feature && feature->contains(point)) {
            result = feature;
        }
    }
    return result;
}

} // namespace index
} // namespace tp::geo

namespace tp::geo::formats {

std::size_t readGeoJSON(tp::geo::FeatureSet& featureSet, const QString& path,
                        const tp::geo::Category* category) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(QString("Unable to open GeoJSON file: %1").arg(path).toStdString());
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        throw std::runtime_error("Invalid GeoJSON document");
    }

    std::size_t count = 0;
    const auto root = document.object();
    const auto features = root.value("type").toString() == "FeatureCollection"
        ? root.value("features").toArray()
        : QJsonArray{root};

    for (const auto& value : features) {
        const auto object = value.toObject();
        const auto geometry = object.value("geometry").toObject();
        if (geometry.isEmpty()) {
            continue;
        }

        auto feature = std::make_shared<tp::geo::Feature>(
            object.value("properties").toObject().value("name").toString(), category);

        const auto properties = object.value("properties").toObject();
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            feature->setAttribute(it.key(), it.value().toVariant().toString());
        }

        readGeometry(*feature, geometry);
        if (!feature->vertices().empty()) {
            featureSet.addFeature(feature);
            ++count;
        }
    }
    return count;
}

bool writeGeoJSON(const QString& path, const tp::geo::Feature& feature, int indent) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonObject properties;
    properties.insert("name", feature.name());
    for (auto it = feature.attributes().begin(); it != feature.attributes().end(); ++it) {
        properties.insert(it.key(), it.value());
    }

    QJsonObject geometry;
    geometry.insert("type", feature.closedPolygon() ? "Polygon" : "LineString");
    if (feature.closedPolygon()) {
        QJsonArray polygon;
        polygon.append(writeCoordinates(feature));
        geometry.insert("coordinates", polygon);
    }
    else {
        geometry.insert("coordinates", writeCoordinates(feature));
    }

    QJsonObject root;
    root.insert("type", "Feature");
    root.insert("properties", properties);
    root.insert("geometry", geometry);
    file.write(QJsonDocument(root).toJson(indent >= 0 ? QJsonDocument::Indented : QJsonDocument::Compact));
    return true;
}

std::size_t writeGeoJSON(const QString& path, const tp::geo::FeatureSet& featureSet, int indent) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return 0;
    }

    QJsonArray features;
    for (const auto& feature : featureSet.features()) {
        if (!feature) {
            continue;
        }

        QJsonObject properties;
        properties.insert("name", feature->name());
        for (auto it = feature->attributes().begin(); it != feature->attributes().end(); ++it) {
            properties.insert(it.key(), it.value());
        }

        QJsonObject geometry;
        geometry.insert("type", feature->closedPolygon() ? "Polygon" : "LineString");
        if (feature->closedPolygon()) {
            QJsonArray polygon;
            polygon.append(writeCoordinates(*feature));
            geometry.insert("coordinates", polygon);
        }
        else {
            geometry.insert("coordinates", writeCoordinates(*feature));
        }

        QJsonObject item;
        item.insert("type", "Feature");
        item.insert("properties", properties);
        item.insert("geometry", geometry);
        features.append(item);
    }

    QJsonObject root;
    root.insert("type", "FeatureCollection");
    root.insert("features", features);
    file.write(QJsonDocument(root).toJson(indent >= 0 ? QJsonDocument::Indented : QJsonDocument::Compact));
    return static_cast<std::size_t>(features.size());
}

std::size_t readBNA(tp::geo::FeatureSet&, const QString&, const tp::geo::Category*) {
    return 0;
}

std::size_t readFEP(tp::geo::FeatureSet&, const QString&, const tp::geo::Category*) {
    return 0;
}

} // namespace tp::geo::formats
