#include "shapeModel.h"

#include "model/creature_geometry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ShapeModel
{
    namespace
    {
        constexpr wchar_t kCreatureFileHeaderV1[] = L"CREATURE_BUILDER_V1";
        constexpr wchar_t kCreatureFileHeaderV2[] = L"CREATURE_BUILDER_V2";
        constexpr wchar_t kCreatureFileHeaderV3[] = L"CREATURE_BUILDER_V3";
        constexpr float kMaxHeadTurnRadians = 3.141592654f;
        constexpr float kSelfAvoidanceLookAhead = 140.0f;
        constexpr float kSelfAvoidanceRadiusBias = 24.0f;
        constexpr float kSelfAvoidanceForwardThreshold = 0.35f;

        bool ValidateConnectivity(const std::vector<Shape>& shapes)
        {
            if (shapes.empty())
            {
                return false;
            }

            std::queue<size_t> pending;
            std::vector<bool> visited(shapes.size(), false);
            pending.push(0);
            visited[0] = true;
            size_t visitedCount = 1;

            while (!pending.empty())
            {
                const size_t currentIndex = pending.front();
                pending.pop();

                for (size_t candidateIndex = 0; candidateIndex < shapes.size(); ++candidateIndex)
                {
                    if (visited[candidateIndex] || currentIndex == candidateIndex)
                    {
                        continue;
                    }

                    if (!shapesTouch(shapes[currentIndex], shapes[candidateIndex]))
                    {
                        continue;
                    }

                    visited[candidateIndex] = true;
                    ++visitedCount;
                    pending.push(candidateIndex);
                }
            }

            return visitedCount == shapes.size();
        }

        std::wstring ShapeTypeToToken(const ShapeType type)
        {
            switch (type)
            {
            case ShapeType::Circle:
                return L"circle";
            case ShapeType::Square:
                return L"square";
            case ShapeType::Triangle:
                return L"triangle";
            }

            return L"unknown";
        }

        bool TryParseShapeType(const std::wstring& token, ShapeType& type)
        {
            if (token == L"circle")
            {
                type = ShapeType::Circle;
                return true;
            }

            if (token == L"square")
            {
                type = ShapeType::Square;
                return true;
            }

            if (token == L"triangle")
            {
                type = ShapeType::Triangle;
                return true;
            }

            return false;
        }

        std::vector<size_t> BuildAnimationOrder(const std::vector<Shape>& shapes)
        {
            if (shapes.empty())
            {
                return {};
            }

            std::unordered_map<int, size_t> idToIndex;
            idToIndex.reserve(shapes.size());
            std::unordered_set<int> pointedToIds;
            for (size_t index = 0; index < shapes.size(); ++index)
            {
                idToIndex.emplace(shapes[index].id, index);
                for (const int nextId : shapes[index].nextShapeIds)
                {
                    if (nextId > 0)
                    {
                        pointedToIds.insert(nextId);
                    }
                }
            }

            size_t headIndex = 0;
            bool foundHead = false;
            for (size_t index = 0; index < shapes.size(); ++index)
            {
                if (pointedToIds.find(shapes[index].id) == pointedToIds.end())
                {
                    if (!foundHead || shapes[index].id < shapes[headIndex].id)
                    {
                        headIndex = index;
                        foundHead = true;
                    }
                }
            }

            if (!foundHead)
            {
                for (size_t index = 1; index < shapes.size(); ++index)
                {
                    if (shapes[index].id < shapes[headIndex].id)
                    {
                        headIndex = index;
                    }
                }
            }

            std::vector<size_t> order;
            order.reserve(shapes.size());
            std::vector<bool> visited(shapes.size(), false);

            std::queue<size_t> pending;
            pending.push(headIndex);

            while (!pending.empty())
            {
                const size_t currentIndex = pending.front();
                pending.pop();
                if (visited[currentIndex])
                {
                    continue;
                }

                visited[currentIndex] = true;
                order.push_back(currentIndex);

                std::vector<int> nextIds = shapes[currentIndex].nextShapeIds;
                std::sort(nextIds.begin(), nextIds.end());
                for (const int nextId : nextIds)
                {
                    const auto nextIt = idToIndex.find(nextId);
                    if (nextIt != idToIndex.end() && !visited[nextIt->second])
                    {
                        pending.push(nextIt->second);
                    }
                }
            }

            for (size_t index = 0; index < shapes.size(); ++index)
            {
                if (!visited[index])
                {
                    order.push_back(index);
                }
            }

            return order;
        }

        float DotProduct(const Point& left, const Point& right)
        {
            return left.x * right.x + left.y * right.y;
        }

        Point AddPoints(const Point& left, const Point& right)
        {
            return Point{ left.x + right.x, left.y + right.y };
        }

        Point ComputeSelfAvoidanceDirection(const std::vector<Shape>& shapes, size_t headIndex, const Point& headFacing)
        {
            const Shape& head = shapes[headIndex];
            const Point headDirection = normalizeOrDefault(headFacing, Point{ 1.0f, 0.0f });
            Point avoidance{ 0.0f, 0.0f };
            float strongestThreatWeight = 0.0f;

            for (size_t index = 0; index < shapes.size(); ++index)
            {
                if (index == headIndex)
                {
                    continue;
                }

                const Shape& bodyShape = shapes[index];
                const Point awayFromBody = subtractPoints(head.start, bodyShape.start);
                const float distance = pointLength(awayFromBody);
                if (distance <= 0.0001f)
                {
                    continue;
                }

                const Point bodyDirection = scalePoint(awayFromBody, 1.0f / distance);
                const float forwardAlignment = DotProduct(headDirection, bodyDirection);
                if (forwardAlignment < kSelfAvoidanceForwardThreshold)
                {
                    continue;
                }

                const float influenceRadius = computeHalfExtent(head) + computeHalfExtent(bodyShape) + kSelfAvoidanceRadiusBias;
                if (distance > kSelfAvoidanceLookAhead)
                {
                    continue;
                }

                const float proximity = (std::max)(0.0f, influenceRadius - distance);
                const float weight = (proximity / influenceRadius) * (0.5f + forwardAlignment);
                if (weight > strongestThreatWeight)
                {
                    strongestThreatWeight = weight;
                    avoidance = bodyDirection;
                }
            }

            return avoidance;
        }

        bool ValidateNextLinks(const std::vector<Shape>& shapes)
        {
            if (shapes.empty())
            {
                return true;
            }

            std::unordered_set<int> ids;
            ids.reserve(shapes.size());
            for (const Shape& shape : shapes)
            {
                ids.insert(shape.id);
            }

            for (const Shape& shape : shapes)
            {
                std::unordered_set<int> localTargets;
                for (const int nextId : shape.nextShapeIds)
                {
                    if (nextId <= 0 || nextId == shape.id || ids.find(nextId) == ids.end())
                    {
                        return false;
                    }

                    if (!localTargets.insert(nextId).second)
                    {
                        return false;
                    }

                }
            }

            return true;
        }
    }

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

    CommitResult commitDrag(DrawingState& state)
    {
        if (!state.activeState.has_value())
        {
            return CommitResult::NoActiveShape;
        }

        const DragState& dragState = state.activeState.value();

        Shape newShape{
            state.nextShapeId,
            {},
            dragState.type,
            dragState.start,
            dragState.end,
            dragState.style,
            false
        };

        if (!state.shapes.empty())
        {
            std::vector<size_t> parentIndices;
            for (size_t index = 0; index < state.shapes.size(); ++index)
            {
                if (shapesTouch(newShape, state.shapes[index]))
                {
                    parentIndices.push_back(index);
                }
            }

            if (parentIndices.empty())
            {
                state.activeState.reset();
                return CommitResult::Detached;
            }

            for (const size_t parentIndex : parentIndices)
            {
                Shape& parent = state.shapes[parentIndex];
                if (std::find(parent.nextShapeIds.begin(), parent.nextShapeIds.end(), newShape.id) == parent.nextShapeIds.end())
                {
                    parent.nextShapeIds.push_back(newShape.id);
                }
            }
        }

        state.shapes.push_back(newShape);
        state.nextShapeId++;
        state.activeState.reset();
        return CommitResult::Added;
    }

    void cancelDrag(DrawingState& state)
    {
        state.activeState.reset();
    }

    bool canCommitActiveShape(const DrawingState& state)
    {
        if (!state.activeState.has_value())
        {
            return false;
        }

        if (state.shapes.empty())
        {
            return true;
        }

        const DragState& dragState = state.activeState.value();
        const Shape previewShape{
            state.nextShapeId,
            {},
            dragState.type,
            dragState.start,
            dragState.end,
            dragState.style,
            false
        };

        for (const Shape& existingShape : state.shapes)
        {
            if (shapesTouch(previewShape, existingShape))
            {
                return true;
            }
        }

        return false;
    }

    bool isCreatureConnected(const DrawingState& state)
    {
        return ValidateConnectivity(state.shapes);
    }

    void setAnimationPlaying(DrawingState& state, const bool isPlaying)
    {
        state.animation.isPlaying = isPlaying;
    }

    bool isAnimationPlaying(const DrawingState& state)
    {
        return state.animation.isPlaying;
    }

    void setAnimationTarget(DrawingState& state, const Point target)
    {
        state.animation.target = target;
        state.animation.hasTarget = true;
    }

    void advanceAnimation(DrawingState& state, const float deltaSeconds, const float viewportWidth, const float viewportHeight)
    {
        if (!state.animation.isPlaying || state.shapes.empty() || deltaSeconds <= 0.0f)
        {
            return;
        }

        if (!state.animation.hasTarget)
        {
            return;
        }

        const float width = (std::max)(1.0f, viewportWidth);
        const float height = (std::max)(1.0f, viewportHeight);
        const std::vector<size_t> animationOrder = BuildAnimationOrder(state.shapes);
        std::unordered_map<int, size_t> idToIndex;
        idToIndex.reserve(state.shapes.size());
        for (size_t index = 0; index < state.shapes.size(); ++index)
        {
            idToIndex.emplace(state.shapes[index].id, index);
        }

        std::vector<int> parentIndexByShape(state.shapes.size(), -1);
        for (size_t parentIndex = 0; parentIndex < state.shapes.size(); ++parentIndex)
        {
            const Shape& parentShape = state.shapes[parentIndex];
            for (const int nextId : parentShape.nextShapeIds)
            {
                const auto childIt = idToIndex.find(nextId);
                if (childIt == idToIndex.end())
                {
                    continue;
                }

                const size_t childIndex = childIt->second;
                const int currentParentIndex = parentIndexByShape[childIndex];
                if (currentParentIndex < 0 || state.shapes[parentIndex].id < state.shapes[static_cast<size_t>(currentParentIndex)].id)
                {
                    parentIndexByShape[childIndex] = static_cast<int>(parentIndex);
                }
            }
        }

        Shape& head = state.shapes[animationOrder.front()];
        const Point previousHeadCenter = head.start;
        const Point currentFacing = subtractPoints(head.end, head.start);
        const float headHalfExtent = computeHalfExtent(head);

        const float margin = headHalfExtent + 2.0f;
        const float targetX = (std::max)(margin, (std::min)(width - margin, state.animation.target.x));
        const float targetY = (std::max)(margin, (std::min)(height - margin, state.animation.target.y));

        const Point toTarget{ targetX - previousHeadCenter.x, targetY - previousHeadCenter.y };
        const float distanceToTarget = pointLength(toTarget);
        const Point targetDirection = normalizeOrDefault(toTarget, normalizeOrDefault(currentFacing, Point{ 1.0f, 0.0f }));
        const Point avoidanceDirection = ComputeSelfAvoidanceDirection(state.shapes, animationOrder.front(), currentFacing);

        Point headDelta{ 0.0f, 0.0f };
        if (distanceToTarget > 0.0001f)
        {
            const float maxStep = state.animation.headSpeed * deltaSeconds;
            const float step = (std::min)(maxStep, distanceToTarget);
            headDelta = scalePoint(targetDirection, step);
        }

        translateShape(head, headDelta);
        if (distanceToTarget > 0.0001f)
        {
            Point nextFacing = rotateTowards(currentFacing, targetDirection, kMaxHeadTurnRadians);
            if (pointLength(avoidanceDirection) > 0.0001f)
            {
                nextFacing = rotateAway(nextFacing, avoidanceDirection, kMaxHeadTurnRadians);
            }

            setShapeFacingDirection(head, nextFacing);
        }

        for (size_t chainIndex = 1; chainIndex < animationOrder.size(); ++chainIndex)
        {
            const size_t currentIndex = animationOrder[chainIndex];
            int parentIndex = parentIndexByShape[currentIndex];
            if (parentIndex < 0)
            {
                parentIndex = static_cast<int>(animationOrder[chainIndex - 1]);
            }

            Shape& previousShape = state.shapes[static_cast<size_t>(parentIndex)];
            Shape& currentShape = state.shapes[currentIndex];

            const Point currentToPrevious = subtractPoints(currentShape.start, previousShape.start);
            const Point direction = normalizeOrDefault(currentToPrevious, Point{ -1.0f, 0.0f });

            const float previousRadius = computeHalfExtent(previousShape);
            const float currentRadius = computeHalfExtent(currentShape);
            const float desiredSpacing = (previousRadius + currentRadius) * state.animation.followSpacingScale;

            const Point desiredOffset = scalePoint(direction, desiredSpacing);
            const Point targetCenter{
                previousShape.start.x + desiredOffset.x,
                previousShape.start.y + desiredOffset.y
            };

            const Point movement{
                targetCenter.x - currentShape.start.x,
                targetCenter.y - currentShape.start.y
            };

            translateShape(currentShape, movement);
            if (pointLength(movement) > 0.0001f)
            {
                setShapeFacingDirection(currentShape, movement);
            }
        }
    }

    bool saveCreature(const DrawingState& state, const std::wstring& filePath, std::wstring& errorMessage)
    {
        if (state.shapes.empty())
        {
            errorMessage = L"Add at least one shape before saving a creature.";
            return false;
        }

        if (!isCreatureConnected(state))
        {
            errorMessage = L"Only fully connected creatures can be saved.";
            return false;
        }

        std::wofstream outputStream{ std::filesystem::path(filePath) };
        if (!outputStream.is_open())
        {
            errorMessage = L"The creature file could not be opened for writing.";
            return false;
        }

        outputStream << kCreatureFileHeaderV3 << L'\n';
        outputStream << state.shapes.size() << L'\n';

        for (const Shape& shape : state.shapes)
        {
            outputStream
                << shape.id << L' '
                << ShapeTypeToToken(shape.type) << L' '
                << shape.start.x << L' ' << shape.start.y << L' '
                << shape.end.x << L' ' << shape.end.y << L' '
                << static_cast<int>(shape.style.strokeColour.r) << L' '
                << static_cast<int>(shape.style.strokeColour.g) << L' '
                << static_cast<int>(shape.style.strokeColour.b) << L' '
                << static_cast<int>(shape.style.strokeColour.a) << L' '
                << static_cast<int>(shape.style.fillColour.r) << L' '
                << static_cast<int>(shape.style.fillColour.g) << L' '
                << static_cast<int>(shape.style.fillColour.b) << L' '
                << static_cast<int>(shape.style.fillColour.a) << L' '
                << shape.style.strokeWidth << L' '
                << (shape.style.hasFill ? 1 : 0) << L' '
                << shape.nextShapeIds.size();

            for (const int nextId : shape.nextShapeIds)
            {
                outputStream << L' ' << nextId;
            }

            outputStream << L'\n';
        }

        if (!outputStream.good())
        {
            errorMessage = L"The creature file could not be written completely.";
            return false;
        }

        errorMessage.clear();
        return true;
    }

    bool loadCreature(DrawingState& state, const std::wstring& filePath, std::wstring& errorMessage)
    {
        std::wifstream inputStream{ std::filesystem::path(filePath) };
        if (!inputStream.is_open())
        {
            errorMessage = L"The creature file could not be opened.";
            return false;
        }

        std::wstring header;
        std::getline(inputStream, header);
        const bool isV3File = (header == kCreatureFileHeaderV3);
        const bool isV2File = (header == kCreatureFileHeaderV2);
        const bool isV1File = (header == kCreatureFileHeaderV1);
        if (!isV3File && !isV2File && !isV1File)
        {
            errorMessage = L"This file is not a supported creature file.";
            return false;
        }

        size_t shapeCount = 0;
        if (!(inputStream >> shapeCount) || shapeCount == 0)
        {
            errorMessage = L"The creature file does not contain any shapes.";
            return false;
        }

        std::vector<Shape> loadedShapes;
        loadedShapes.reserve(shapeCount);
        int highestShapeId = 0;
        std::unordered_set<int> usedIds;

        for (size_t shapeIndex = 0; shapeIndex < shapeCount; ++shapeIndex)
        {
            Shape shape{};
            std::wstring shapeTypeToken;
            int strokeRed = 0;
            int strokeGreen = 0;
            int strokeBlue = 0;
            int strokeAlpha = 0;
            int fillRed = 0;
            int fillGreen = 0;
            int fillBlue = 0;
            int fillAlpha = 0;
            int hasFill = 0;
            int nextShapeId = -1;
            int nextShapeCount = 0;

            if (!(inputStream
                >> shape.id
                >> shapeTypeToken
                >> shape.start.x >> shape.start.y
                >> shape.end.x >> shape.end.y
                >> strokeRed >> strokeGreen >> strokeBlue >> strokeAlpha
                >> fillRed >> fillGreen >> fillBlue >> fillAlpha
                >> shape.style.strokeWidth
                >> hasFill))
            {
                errorMessage = L"The creature file is incomplete or malformed.";
                return false;
            }

            if (isV2File)
            {
                if (!(inputStream >> nextShapeId))
                {
                    errorMessage = L"The creature file is incomplete or malformed.";
                    return false;
                }
            }

            if (!TryParseShapeType(shapeTypeToken, shape.type))
            {
                errorMessage = L"The creature file contains an unknown shape type.";
                return false;
            }

            if (shape.id <= 0 || !usedIds.insert(shape.id).second)
            {
                errorMessage = L"The creature file contains invalid shape identifiers.";
                return false;
            }

            shape.style.strokeColour = Colour{
                static_cast<uint8_t>(strokeRed),
                static_cast<uint8_t>(strokeGreen),
                static_cast<uint8_t>(strokeBlue),
                static_cast<uint8_t>(strokeAlpha)
            };
            shape.style.fillColour = Colour{
                static_cast<uint8_t>(fillRed),
                static_cast<uint8_t>(fillGreen),
                static_cast<uint8_t>(fillBlue),
                static_cast<uint8_t>(fillAlpha)
            };
            shape.style.hasFill = hasFill != 0;
            shape.nextShapeIds.clear();

            if (isV3File)
            {
                if (!(inputStream >> nextShapeCount) || nextShapeCount < 0)
                {
                    errorMessage = L"The creature file is incomplete or malformed.";
                    return false;
                }

                shape.nextShapeIds.reserve(static_cast<size_t>(nextShapeCount));
                for (int nextIndex = 0; nextIndex < nextShapeCount; ++nextIndex)
                {
                    int linkedId = -1;
                    if (!(inputStream >> linkedId))
                    {
                        errorMessage = L"The creature file is incomplete or malformed.";
                        return false;
                    }

                    shape.nextShapeIds.push_back(linkedId);
                }
            }
            else if (isV2File && nextShapeId > 0)
            {
                shape.nextShapeIds.push_back(nextShapeId);
            }

            shape.selected = false;

            loadedShapes.push_back(shape);
            highestShapeId = (std::max)(highestShapeId, shape.id);
        }

        if (!ValidateConnectivity(loadedShapes))
        {
            errorMessage = L"The creature file contains loose parts, so it cannot be loaded.";
            return false;
        }

        if (!ValidateNextLinks(loadedShapes))
        {
            errorMessage = L"The creature file contains invalid fixed next-shape links.";
            return false;
        }

        state.shapes = std::move(loadedShapes);
        state.activeState.reset();
        state.nextShapeId = highestShapeId + 1;
        errorMessage.clear();
        return true;
    }
}