#include "engineloader.h"

#include "shapeModel.h"

#include <gl/GL.h>

#include <algorithm>
#include <cmath>

namespace EngineLoader
{
namespace
{
HGLRC g_glContext = nullptr;
HDC g_deviceContext = nullptr;

constexpr float kColorScale = 1.0f / 255.0f;
constexpr float kMinHalfExtent = 4.0f;
constexpr int kCircleSegments = 48;

float ToFloatColor(uint8_t value)
{
    return static_cast<float>(value) * kColorScale;
}

void SetColor(const ShapeModel::Colour& color)
{
    glColor4f(ToFloatColor(color.r), ToFloatColor(color.g), ToFloatColor(color.b), ToFloatColor(color.a));
}

float ComputeHalfExtent(const ShapeModel::Point& start, const ShapeModel::Point& end)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    return (std::max)(kMinHalfExtent, (std::max)(std::fabs(dx), std::fabs(dy)));
}

void DrawSquare(GLenum mode, const ShapeModel::Point& center, float halfExtent)
{
    glBegin(mode);
    glVertex2f(center.x - halfExtent, center.y - halfExtent);
    glVertex2f(center.x + halfExtent, center.y - halfExtent);
    glVertex2f(center.x + halfExtent, center.y + halfExtent);
    glVertex2f(center.x - halfExtent, center.y + halfExtent);
    glEnd();
}

void DrawTriangle(GLenum mode, const ShapeModel::Point& center, float halfExtent)
{
    glBegin(mode);
    glVertex2f(center.x, center.y - halfExtent);
    glVertex2f(center.x - halfExtent, center.y + halfExtent);
    glVertex2f(center.x + halfExtent, center.y + halfExtent);
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

void DrawShapePrimitive(const ShapeModel::ShapeType type, GLenum mode, const ShapeModel::Point& center, float halfExtent)
{
    switch (type)
    {
    case ShapeModel::ShapeType::Circle:
        DrawCircle(mode, center, halfExtent);
        break;
    case ShapeModel::ShapeType::Square:
        DrawSquare(mode, center, halfExtent);
        break;
    case ShapeModel::ShapeType::Triangle:
        DrawTriangle(mode, center, halfExtent);
        break;
    }
}

void DrawShape(const ShapeModel::Shape& shape)
{
    const float halfExtent = ComputeHalfExtent(shape.start, shape.end);

    if (shape.style.hasFill)
    {
        SetColor(shape.style.fillColour);
        DrawShapePrimitive(shape.type, GL_TRIANGLE_FAN, shape.start, halfExtent);
    }

    if (shape.style.strokeWidth > 0.0f)
    {
        glLineWidth(shape.style.strokeWidth);
        SetColor(shape.style.strokeColour);
        DrawShapePrimitive(shape.type, GL_LINE_LOOP, shape.start, halfExtent);
    }
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

    const float halfExtent = ComputeHalfExtent(dragState.start, dragState.end);

    if (previewStyle.hasFill)
    {
        SetColor(previewStyle.fillColour);
        DrawShapePrimitive(dragState.type, GL_TRIANGLE_FAN, dragState.start, halfExtent);
    }

    if (previewStyle.strokeWidth > 0.0f)
    {
        glLineWidth(previewStyle.strokeWidth);
        SetColor(previewStyle.strokeColour);
        DrawShapePrimitive(dragState.type, GL_LINE_LOOP, dragState.start, halfExtent);
    }
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