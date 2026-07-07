#pragma once

#include <windows.h>

namespace EngineLoader
{
bool CreateOpenGLContext(HWND window);
void DestroyOpenGLContext(HWND window);
void RenderFrame(int width, int height);
bool HasActiveContext();
}