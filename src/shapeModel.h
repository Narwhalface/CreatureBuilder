#ifndef SHAPEMODEL_H
#define SHAPEMODEL_H

#include <vector>
#include <optional>
#include <cstdint>

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
        std::vector<Shape> shapes;

        std::optional<DragState> activeState;

        ShapeType currentTool = ShapeType::Circle;
        ShapeStyle currentStyle;

        int nextShapeId = 1;
    };

    void beginDrag(DrawingState& state, Point start);
    void updateDrag(DrawingState& state, Point end);
    void commitDrag(DrawingState& state);
    void cancelDrag(DrawingState& state);
}

#endif