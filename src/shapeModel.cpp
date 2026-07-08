#include "shapeModel.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_set>

namespace ShapeModel
{
    namespace
    {
        constexpr wchar_t kCreatureFileHeader[] = L"CREATURE_BUILDER_V1";
        constexpr float kMinimumHalfExtent = 4.0f;
        constexpr float kTouchEpsilon = 0.5f;

        struct Bounds
        {
            float left = 0.0f;
            float top = 0.0f;
            float right = 0.0f;
            float bottom = 0.0f;
        };

        float ComputeHalfExtent(const Shape& shape)
        {
            const float dx = shape.end.x - shape.start.x;
            const float dy = shape.end.y - shape.start.y;
            return (std::max)(kMinimumHalfExtent, (std::max)(std::fabs(dx), std::fabs(dy)));
        }

        Bounds ComputeBounds(const Shape& shape)
        {
            const float halfExtent = ComputeHalfExtent(shape);
            return Bounds{
                shape.start.x - halfExtent,
                shape.start.y - halfExtent,
                shape.start.x + halfExtent,
                shape.start.y + halfExtent
            };
        }

        bool BoundsTouch(const Bounds& left, const Bounds& right)
        {
            const bool separatedHorizontally = left.right < right.left - kTouchEpsilon || right.right < left.left - kTouchEpsilon;
            const bool separatedVertically = left.bottom < right.top - kTouchEpsilon || right.bottom < left.top - kTouchEpsilon;
            return !separatedHorizontally && !separatedVertically;
        }

        bool ShapesTouch(const Shape& left, const Shape& right)
        {
            return BoundsTouch(ComputeBounds(left), ComputeBounds(right));
        }

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

                    if (!ShapesTouch(shapes[currentIndex], shapes[candidateIndex]))
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
            dragState.type,
            dragState.start,
            dragState.end,
            dragState.style,
            false
        };

        if (!state.shapes.empty())
        {
            bool touchesExistingShape = false;
            for (const Shape& existingShape : state.shapes)
            {
                if (ShapesTouch(newShape, existingShape))
                {
                    touchesExistingShape = true;
                    break;
                }
            }

            if (!touchesExistingShape)
            {
                state.activeState.reset();
                return CommitResult::Detached;
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
            dragState.type,
            dragState.start,
            dragState.end,
            dragState.style,
            false
        };

        for (const Shape& existingShape : state.shapes)
        {
            if (ShapesTouch(previewShape, existingShape))
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

        outputStream << kCreatureFileHeader << L'\n';
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
                << (shape.style.hasFill ? 1 : 0) << L'\n';
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
        if (header != kCreatureFileHeader)
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
            shape.selected = false;

            loadedShapes.push_back(shape);
            highestShapeId = (std::max)(highestShapeId, shape.id);
        }

        if (!ValidateConnectivity(loadedShapes))
        {
            errorMessage = L"The creature file contains loose parts, so it cannot be loaded.";
            return false;
        }

        state.shapes = std::move(loadedShapes);
        state.activeState.reset();
        state.nextShapeId = highestShapeId + 1;
        errorMessage.clear();
        return true;
    }
}