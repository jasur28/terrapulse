#pragma once

#include "terrapulse/geo/coordinate.h"

namespace tp::geo {

class BoundingBox {
public:
    using ValueType = Coordinate::ValueType;

    enum class Relation {
        Disjoint,
        Contains,
        Intersects
    };

    BoundingBox();
    BoundingBox(ValueType south, ValueType west, ValueType north, ValueType east);

    bool isEmpty() const { return m_empty; }
    bool isNull() const;
    void reset();
    BoundingBox& normalize();

    bool crossesDateLine() const;
    bool coversFullLongitude() const;
    ValueType width() const;
    ValueType height() const;
    Coordinate center() const;

    bool contains(const Coordinate& value) const;
    bool contains(const BoundingBox& other) const;
    bool intersects(const BoundingBox& other) const;
    Relation relation(const BoundingBox& other) const;
    void merge(const BoundingBox& other);
    void expand(const Coordinate& value);
    void fromPolygon(const Coordinate* coords, std::size_t count);

    ValueType north = 0.0;
    ValueType south = 0.0;
    ValueType east = 0.0;
    ValueType west = 0.0;

private:
    bool m_empty = true;
};

} // namespace tp::geo
