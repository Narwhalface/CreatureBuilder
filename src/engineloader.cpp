#include "engineloader.h"

#include "model/creature_geometry.h"
#include "shapeModel.h"

#include <gl/GL.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace EngineLoader
{
namespace
{
HGLRC g_glContext = nullptr;
HDC g_deviceContext = nullptr;

constexpr float kColorScale = 1.0f / 255.0f;
constexpr int kCircleSegments = 48;

float ToFloatColor(uint8_t value)
{
    return static_cast<float>(value) * kColorScale;
}

void SetColor(const ShapeModel::Colour& color)
{
    glColor4f(ToFloatColor(color.r), ToFloatColor(color.g), ToFloatColor(color.b), ToFloatColor(color.a));
}

ShapeModel::Point RotateLocalPoint(const ShapeModel::Point& point, float rotationRadians)
{
    const float c = std::cos(rotationRadians);
    const float s = std::sin(rotationRadians);
    return ShapeModel::Point{
        point.x * c - point.y * s,
        point.x * s + point.y * c
    };
}

float ComputeRotationRadians(const ShapeModel::Point& start, const ShapeModel::Point& end)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    if ((std::fabs(dx) + std::fabs(dy)) < 0.0001f)
    {
        return 0.0f;
    }

    return std::atan2(dy, dx);
}

void DrawSquare(GLenum mode, const ShapeModel::Point& center, float halfExtent, float rotationRadians)
{
    const ShapeModel::Point vertices[4] = {
        ShapeModel::Point{ -halfExtent, -halfExtent },
        ShapeModel::Point{ halfExtent, -halfExtent },
        ShapeModel::Point{ halfExtent, halfExtent },
        ShapeModel::Point{ -halfExtent, halfExtent }
    };

    glBegin(mode);
    for (const ShapeModel::Point& vertex : vertices)
    {
        const ShapeModel::Point rotated = RotateLocalPoint(vertex, rotationRadians);
        glVertex2f(center.x + rotated.x, center.y + rotated.y);
    }
    glEnd();
}

void DrawTriangle(GLenum mode, const ShapeModel::Point& center, float halfExtent, float rotationRadians)
{
    const ShapeModel::Point vertices[3] = {
        ShapeModel::Point{ halfExtent, 0.0f },
        ShapeModel::Point{ -halfExtent, -halfExtent },
        ShapeModel::Point{ -halfExtent, halfExtent }
    };

    glBegin(mode);
    for (const ShapeModel::Point& vertex : vertices)
    {
        const ShapeModel::Point rotated = RotateLocalPoint(vertex, rotationRadians);
        glVertex2f(center.x + rotated.x, center.y + rotated.y);
    }
    glEnd();
}

void DrawCircle(GLenum mode, const ShapeModel::Point& center, float halfExtent)
{
    if (mode == GL_TRIANGLE_FAN)
    {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(center.x, center.y);

        for (int segmentIndex = 0; segmentIndex <= kCircleSegments; ++segmentIndex)
        {
            const float fraction = static_cast<float>(segmentIndex) / static_cast<float>(kCircleSegments);
            const float angle = fraction * 6.28318530718f;
            const float px = center.x + std::cos(angle) * halfExtent;
            const float py = center.y + std::sin(angle) * halfExtent;
            glVertex2f(px, py);
        }

        glEnd();
        return;
    }

    glBegin(GL_LINE_LOOP);
    for (int segmentIndex = 0; segmentIndex < kCircleSegments; ++segmentIndex)
    {
        const float fraction = static_cast<float>(segmentIndex) / static_cast<float>(kCircleSegments);
        const float angle = fraction * 6.28318530718f;
        const float px = center.x + std::cos(angle) * halfExtent;
        const float py = center.y + std::sin(angle) * halfExtent;
        glVertex2f(px, py);
    }
    glEnd();
}

void DrawShapePrimitive(const ShapeModel::ShapeType type, GLenum mode, const ShapeModel::Point& center, float halfExtent, float rotationRadians)
{
    switch (type)
    {
    case ShapeModel::ShapeType::Circle:
        DrawCircle(mode, center, halfExtent);
        break;
    case ShapeModel::ShapeType::Square:
        DrawSquare(mode, center, halfExtent, rotationRadians);
        break;
    case ShapeModel::ShapeType::Triangle:
        DrawTriangle(mode, center, halfExtent, rotationRadians);
        break;
    }
}

void DrawFacingDebugLine(const ShapeModel::Point& center, const ShapeModel::Point& facingDirection, float halfExtent)
{
    const ShapeModel::Point direction = ShapeModel::normalizeOrDefault(facingDirection, ShapeModel::Point{ 1.0f, 0.0f });
    const float lineLength = halfExtent * 1.25f;

    SetColor(ShapeModel::Colour{ 70, 255, 70, 220 });
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(center.x, center.y);
    glVertex2f(center.x + direction.x * lineLength, center.y + direction.y * lineLength);
    glEnd();
}

void DrawShape(const ShapeModel::Shape& shape)
{
    const float halfExtent = ShapeModel::computeHalfExtent(shape);
    const float rotationRadians = ComputeRotationRadians(shape.start, shape.end);

    if (shape.style.hasFill)
    {
        SetColor(shape.style.fillColour);
        DrawShapePrimitive(shape.type, GL_TRIANGLE_FAN, shape.start, halfExtent, rotationRadians);
    }

    if (shape.style.strokeWidth > 0.0f)
    {
        glLineWidth(shape.style.strokeWidth);
        SetColor(shape.style.strokeColour);
        DrawShapePrimitive(shape.type, GL_LINE_LOOP, shape.start, halfExtent, rotationRadians);
    }

    DrawFacingDebugLine(shape.start, ShapeModel::subtractPoints(shape.end, shape.start), halfExtent);
}

void DrawDragPreview(const ShapeModel::DrawingState& drawingState)
{
    if (!drawingState.activeState.has_value())
    {
        return;
    }

    const ShapeModel::DragState& dragState = drawingState.activeState.value();
    ShapeModel::ShapeStyle previewStyle = dragState.style;
    const bool previewIsValid = ShapeModel::canCommitActiveShape(drawingState);
    const float rotationRadians = ComputeRotationRadians(dragState.start, dragState.end);

    if (previewIsValid)
    {
        previewStyle.fillColour.r = static_cast<uint8_t>((std::min)(255, previewStyle.fillColour.r + 30));
        previewStyle.fillColour.g = static_cast<uint8_t>((std::min)(255, previewStyle.fillColour.g + 30));
        previewStyle.fillColour.b = static_cast<uint8_t>((std::min)(255, previewStyle.fillColour.b + 30));
        previewStyle.strokeColour.r = static_cast<uint8_t>((std::min)(255, previewStyle.strokeColour.r + 20));
        previewStyle.strokeColour.g = static_cast<uint8_t>((std::min)(255, previewStyle.strokeColour.g + 20));
        previewStyle.strokeColour.b = static_cast<uint8_t>((std::min)(255, previewStyle.strokeColour.b + 20));
    }
    else
    {
        previewStyle.fillColour = ShapeModel::Colour{190, 65, 65, 170};
        previewStyle.strokeColour = ShapeModel::Colour{255, 220, 220, 255};
    }

    const float halfExtent = ShapeModel::computeHalfExtent(ShapeModel::Shape{
        0,
        {},
        dragState.type,
        dragState.start,
        dragState.end,
        dragState.style,
        false
    });

    if (previewStyle.hasFill)
    {
        SetColor(previewStyle.fillColour);
        DrawShapePrimitive(dragState.type, GL_TRIANGLE_FAN, dragState.start, halfExtent, rotationRadians);
    }

    if (previewStyle.strokeWidth > 0.0f)
    {
        glLineWidth(previewStyle.strokeWidth);
        SetColor(previewStyle.strokeColour);
        DrawShapePrimitive(dragState.type, GL_LINE_LOOP, dragState.start, halfExtent, rotationRadians);
    }

    DrawFacingDebugLine(dragState.start, ShapeModel::subtractPoints(dragState.end, dragState.start), halfExtent);
}

void DrawLinkDebugLines(const ShapeModel::DrawingState& drawingState)
{
    if (drawingState.shapes.empty())
    {
        return;
    }

    std::unordered_map<int, const ShapeModel::Shape*> idToShape;
    idToShape.reserve(drawingState.shapes.size());
    for (const ShapeModel::Shape& shape : drawingState.shapes)
    {
        idToShape.emplace(shape.id, &shape);
    }

    glLineWidth(1.5f);
    glBegin(GL_LINES);
    for (const ShapeModel::Shape& parent : drawingState.shapes)
    {
        for (const int childId : parent.nextShapeIds)
        {
            const auto childIt = idToShape.find(childId);
            if (childIt == idToShape.end())
            {
                continue;
            }

            const ShapeModel::Shape* child = childIt->second;
            SetColor(ShapeModel::Colour{90, 230, 255, 210});
            glVertex2f(parent.start.x, parent.start.y);
            glVertex2f(child->start.x, child->start.y);
        }
    }
    glEnd();
}
}

bool CreateOpenGLContext(HWND window)
{
    g_deviceContext = GetDC(window);
    if (g_deviceContext == nullptr)
    {
        return false;
    }

    PIXELFORMATDESCRIPTOR pixelFormatDescriptor{};
    pixelFormatDescriptor.nSize = sizeof(pixelFormatDescriptor);
    pixelFormatDescriptor.nVersion = 1;
    pixelFormatDescriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormatDescriptor.iPixelType = PFD_TYPE_RGBA;
    pixelFormatDescriptor.cColorBits = 32;
    pixelFormatDescriptor.cDepthBits = 24;
    pixelFormatDescriptor.cStencilBits = 8;
    pixelFormatDescriptor.iLayerType = PFD_MAIN_PLANE;

    const int pixelFormat = ChoosePixelFormat(g_deviceContext, &pixelFormatDescriptor);
    if (pixelFormat == 0)
    {
        return false;
    }

    if (SetPixelFormat(g_deviceContext, pixelFormat, &pixelFormatDescriptor) == FALSE)
    {
        return false;
    }

    g_glContext = wglCreateContext(g_deviceContext);
    if (g_glContext == nullptr)
    {
        return false;
    }

    if (wglMakeCurrent(g_deviceContext, g_glContext) == FALSE)
    {
        return false;
    }

    return true;
}

void DestroyOpenGLContext(HWND window)
{
    if (wglGetCurrentContext() == g_glContext)
    {
        wglMakeCurrent(nullptr, nullptr);
    }

    if (g_glContext != nullptr)
    {
        wglDeleteContext(g_glContext);
        g_glContext = nullptr;
    }

    if (g_deviceContext != nullptr)
    {
        ReleaseDC(window, g_deviceContext);
        g_deviceContext = nullptr;
    }
}

void RenderFrame(int width, int height, const ShapeModel::DrawingState& drawingState)
{
    if (height <= 0)
    {
        height = 1;
    }

    if (!HasActiveContext())
    {
        return;
    }

    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<GLdouble>(width), static_cast<GLdouble>(height), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (const ShapeModel::Shape& shape : drawingState.shapes)
    {
        DrawShape(shape);
    }

    DrawLinkDebugLines(drawingState);

    if (drawingState.activeState.has_value())
    {
        DrawDragPreview(drawingState);
    }

    SwapBuffers(g_deviceContext);
}

bool HasActiveContext()
{
    return g_deviceContext != nullptr && g_glContext != nullptr;
}
}