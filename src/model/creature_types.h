#ifndef CREATURE_TYPES_H
#define CREATURE_TYPES_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ShapeModel
{
    enum class ShapeType
    {
        Circle,
        Square,
        Triangle
    };

    struct Point
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Colour
    {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 255;
    };

    struct ShapeStyle
    {
        Colour strokeColour;
        Colour fillColour;
        float strokeWidth = 1.0f;
        bool hasFill = true;
    };

    struct Shape
    {
        int id = 0;
        std::vector<int> nextShapeIds;
        ShapeType type;
        Point start;
        Point end;
        ShapeStyle style;
        bool selected = false;
    };

    struct DragState
    {
        ShapeType type;
        Point start;
        Point end;
        ShapeStyle style;
    };

    struct DrawingState
    {
        struct AnimationState
        {
            bool isPlaying = false;
            float headSpeed = 150.0f;
            float followSpacingScale = 0.82f;
            Point target{ 0.0f, 0.0f };
            bool hasTarget = false;
        };

        std::vector<Shape> shapes;

        std::optional<DragState> activeState;

        ShapeType currentTool = ShapeType::Circle;
        ShapeStyle currentStyle;

        int nextShapeId = 1;
        AnimationState animation;
    };

    enum class CommitResult
    {
        Added,
        NoActiveShape,
        Detached
    };
}

#endif