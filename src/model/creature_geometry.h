#ifndef CREATURE_GEOMETRY_H
#define CREATURE_GEOMETRY_H

#include "creature_types.h"

namespace ShapeModel
{
    float computeHalfExtent(const Shape& shape);
    Point subtractPoints(const Point& left, const Point& right);
    Point scalePoint(const Point& value, float scale);
    float pointLength(const Point& value);
    Point normalizeOrDefault(const Point& value, const Point& fallback);
    Point rotateTowards(const Point& currentDirection, const Point& desiredDirection, float maxRadians);
    Point rotateAway(const Point& currentDirection, const Point& avoidDirection, float maxRadians);
    float computeRotationRadians(const Point& start, const Point& end);
    void translateShape(Shape& shape, const Point& delta);
    void setShapeFacingDirection(Shape& shape, const Point& desiredDirection);
    bool shapesTouch(const Shape& left, const Shape& right);
}

#endif