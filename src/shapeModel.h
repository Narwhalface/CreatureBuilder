#ifndef SHAPEMODEL_H
#define SHAPEMODEL_H

#include <string>

#include "model/creature_types.h"

namespace ShapeModel
{
    void beginDrag(DrawingState& state, Point start);
    void updateDrag(DrawingState& state, Point end);
    CommitResult commitDrag(DrawingState& state);
    void cancelDrag(DrawingState& state);

    bool canCommitActiveShape(const DrawingState& state);
    bool isCreatureConnected(const DrawingState& state);
    void setAnimationPlaying(DrawingState& state, bool isPlaying);
    bool isAnimationPlaying(const DrawingState& state);
    void setAnimationTarget(DrawingState& state, Point target);
    void advanceAnimation(DrawingState& state, float deltaSeconds, float viewportWidth, float viewportHeight);
    bool saveCreature(const DrawingState& state, const std::wstring& filePath, std::wstring& errorMessage);
    bool loadCreature(DrawingState& state, const std::wstring& filePath, std::wstring& errorMessage);
}

#endif