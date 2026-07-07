#include "engineloader.h"

#include <gl/GL.h>

namespace EngineLoader
{
namespace
{
HGLRC g_glContext = nullptr;
HDC g_deviceContext = nullptr;
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

void RenderFrame(int width, int height)
{
    if (height <= 0)
    {
        height = 1;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    SwapBuffers(g_deviceContext);
}

bool HasActiveContext()
{
    return g_deviceContext != nullptr && g_glContext != nullptr;
}
}