#include "shapeModel.h"

namespace ShapeModel
{
    void beginDrag(DrawingState& state, Point startPoint)
    {
        state.activeState = DragState{
            state.currentTool,
            startPoint,
            startPoint,
            state.currentStyle
        };
    }

    void updateDrag(DrawingState& state, Point currentPoint)
    {
        if (state.activeState)
        {
            state.activeState->end = currentPoint;
        }
    }

    void commitDrag(DrawingState& state)
    {
        if (!state.activeState.has_value())
        {
            return;
        }

        const DragState& dragState = state.activeState.value();

        Shape newShape{
            state.nextShapeId,
            dragState.type,
            dragState.start,
            dragState.end,
            dragState.style,
            false
        };

        state.shapes.push_back(newShape);
        state.nextShapeId++;
        state.activeState.reset();
    }

    void cancelDrag(DrawingState& state)
    {
        state.activeState.reset();
    }
}