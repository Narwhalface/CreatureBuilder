#include <windows.h>

#include "engineloader.h"

namespace
{
constexpr wchar_t kWindowClassName[] = L"CreatureBuilderOpenGLWindow";
constexpr wchar_t kWindowTitle[] = L"CreatureBuilder OpenGL Shell";
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    {
        if (EngineLoader::HasActiveContext())
        {
            const int width = LOWORD(lParam);
            const int height = HIWORD(lParam);
            EngineLoader::RenderFrame(width, height);
        }
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);

        RECT clientRect{};
        GetClientRect(window, &clientRect);
    EngineLoader::RenderFrame(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instanceHandle, HINSTANCE, PWSTR, int showCommand)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&windowClass) == 0)
    {
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        720,
        nullptr,
        nullptr,
        instanceHandle,
        nullptr);

    if (window == nullptr)
    {
        return 1;
    }

    if (!EngineLoader::CreateOpenGLContext(window))
    {
        DestroyWindow(window);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    RECT clientRect{};
    GetClientRect(window, &clientRect);
    EngineLoader::RenderFrame(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    EngineLoader::DestroyOpenGLContext(window);
    return static_cast<int>(message.wParam);
}