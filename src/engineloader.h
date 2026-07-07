#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

namespace ShapeModel
{
struct DrawingState;
}

namespace EngineLoader
{
bool CreateOpenGLContext(HWND window);
void DestroyOpenGLContext(HWND window);
void RenderFrame(int width, int height, const ShapeModel::DrawingState& drawingState);
bool HasActiveContext();
}