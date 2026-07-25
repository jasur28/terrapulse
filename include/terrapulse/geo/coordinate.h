#pragma once

#include <QString>

namespace tp::geo {

class Coordinate {
public:
    using ValueType = double;

    Coordinate() = default;
    Coordinate(ValueType latitude, ValueType longitude) : lat(latitude), lon(longitude) {}

    void set(ValueType latitude, ValueType longitude) {
        lat = latitude;
        lon = longitude;
    }

    ValueType latitude() const { return lat; }
    ValueType longitude() const { return lon; }

    Coordinate& normalize();
    bool isValid() const;
    QString toString() const;

    bool operator==(const Coordinate& other) const { return lat == other.lat && lon == other.lon; }
    bool operator!=(const Coordinate& other) const { return !(*this == other); }

    static ValueType normalizeLat(ValueType latitude);
    static ValueType normalizeLon(ValueType longitude);
    static void normalizeLatLon(ValueType& latitude, ValueType& longitude);
    static ValueType longitudeWidth(ValueType west, ValueType east);
    static ValueType distanceLongitude(ValueType lon0, ValueType lon1);
    static double distanceKm(const Coordinate& a, const Coordinate& b);

    ValueType lat = 0.0;
    ValueType lon = 0.0;
};

using Vertex = Coordinate;

bool contains(const Coordinate& point, const Coordinate* polygon, std::size_t sides);
double area(const Coordinate* polygon, std::size_t sides);

} // namespace tp::geo
