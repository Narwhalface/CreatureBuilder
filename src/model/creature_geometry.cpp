#include "creature_geometry.h"

#include <algorithm>
#include <cmath>

namespace ShapeModel
{
    namespace
    {
        constexpr float kMinimumHalfExtent = 4.0f;
        constexpr float kTouchEpsilon = 0.5f;

        struct Bounds
        {
            float left = 0.0f;
            float top = 0.0f;
            float right = 0.0f;
            float bottom = 0.0f;
        };

        Bounds computeBounds(const Shape& shape)
        {
            const float halfExtent = computeHalfExtent(shape);
            return Bounds{
                shape.start.x - halfExtent,
                shape.start.y - halfExtent,
                shape.start.x + halfExtent,
                shape.start.y + halfExtent
            };
        }

        bool boundsTouch(const Bounds& left, const Bounds& right)
        {
            const bool separatedHorizontally = left.right < right.left - kTouchEpsilon || right.right < left.left - kTouchEpsilon;
            const bool separatedVertically = left.bottom < right.top - kTouchEpsilon || right.bottom < left.top - kTouchEpsilon;
            return !separatedHorizontally && !separatedVertically;
        }
    }

    float computeHalfExtent(const Shape& shape)
    {
        const float dx = shape.end.x - shape.start.x;
        const float dy = shape.end.y - shape.start.y;
        return (std::max)(kMinimumHalfExtent, (std::max)(std::fabs(dx), std::fabs(dy)));
    }

    Point subtractPoints(const Point& left, const Point& right)
    {
        return Point{ left.x - right.x, left.y - right.y };
    }

    Point scalePoint(const Point& value, float scale)
    {
        return Point{ value.x * scale, value.y * scale };
    }

    float pointLength(const Point& value)
    {
        return std::sqrt(value.x * value.x + value.y * value.y);
    }

    Point normalizeOrDefault(const Point& value, const Point& fallback)
    {
        const float length = pointLength(value);
        if (length <= 0.0001f)
        {
            return fallback;
        }

        return Point{ value.x / length, value.y / length };
    }

    Point rotateTowards(const Point& currentDirection, const Point& desiredDirection, const float maxRadians)
    {
        const Point current = normalizeOrDefault(currentDirection, Point{ 1.0f, 0.0f });
        const Point desired = normalizeOrDefault(desiredDirection, current);

        const float currentAngle = std::atan2(current.y, current.x);
        const float desiredAngle = std::atan2(desired.y, desired.x);

        float angleDelta = desiredAngle - currentAngle;
        while (angleDelta > 3.1415926535f)
        {
            angleDelta -= 6.283185307f;
        }
        while (angleDelta < -3.1415926535f)
        {
            angleDelta += 6.283185307f;
        }

        const float clampedDelta = (std::max)(-maxRadians, (std::min)(maxRadians, angleDelta));
        const float nextAngle = currentAngle + clampedDelta;
        return Point{ std::cos(nextAngle), std::sin(nextAngle) };
    }

    Point rotateAway(const Point& currentDirection, const Point& avoidDirection, const float maxRadians)
    {
        const Point current = normalizeOrDefault(currentDirection, Point{ 1.0f, 0.0f });
        const Point away = normalizeOrDefault(Point{ -avoidDirection.x, -avoidDirection.y }, current);
        return rotateTowards(current, away, maxRadians);
    }

    float computeRotationRadians(const Point& start, const Point& end)
    {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        if ((std::fabs(dx) + std::fabs(dy)) < 0.0001f)
        {
            return 0.0f;
        }

        return std::atan2(dy, dx);
    }

    void translateShape(Shape& shape, const Point& delta)
    {
        shape.start.x += delta.x;
        shape.start.y += delta.y;
        shape.end.x += delta.x;
        shape.end.y += delta.y;
    }

    void setShapeFacingDirection(Shape& shape, const Point& desiredDirection)
    {
        const float preservedHalfExtent = computeHalfExtent(shape);
        const Point direction = normalizeOrDefault(desiredDirection, Point{ 1.0f, 0.0f });

        const float dominant = (std::max)(0.0001f, (std::max)(std::fabs(direction.x), std::fabs(direction.y)));
        const float scale = preservedHalfExtent / dominant;

        shape.end.x = shape.start.x + direction.x * scale;
        shape.end.y = shape.start.y + direction.y * scale;
    }

    bool shapesTouch(const Shape& left, const Shape& right)
    {
        return boundsTouch(computeBounds(left), computeBounds(right));
    }
}