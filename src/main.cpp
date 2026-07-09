#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>

#include <algorithm>
#include <string>

#include "engineloader.h"
#include "shapeModel.h"

namespace
{
constexpr wchar_t kWindowClassName[] = L"CreatureBuilderOpenGLWindow";
constexpr wchar_t kWindowTitle[] = L"CreatureBuilder OpenGL Shell";
constexpr UINT_PTR kAnimationTimerId = 1;
constexpr UINT kAnimationTimerMs = 16;
ShapeModel::DrawingState g_drawingState;
bool g_isDragging = false;
DWORD g_lastAnimationTickMs = 0;

void UpdateAnimationTarget(ShapeModel::Point point)
{
    ShapeModel::setAnimationTarget(g_drawingState, point);
}

void SetAnimationMode(HWND window, bool isPlaying)
{
    ShapeModel::setAnimationPlaying(g_drawingState, isPlaying);
    if (isPlaying)
    {
        POINT cursorPosition{};
        if (GetCursorPos(&cursorPosition) != FALSE && ScreenToClient(window, &cursorPosition) != FALSE)
        {
            UpdateAnimationTarget(ShapeModel::Point{
                static_cast<float>(cursorPosition.x),
                static_cast<float>(cursorPosition.y)
            });
        }

        g_lastAnimationTickMs = GetTickCount();
        SetTimer(window, kAnimationTimerId, kAnimationTimerMs, nullptr);
    }
    else
    {
        KillTimer(window, kAnimationTimerId);
    }
}

std::wstring CreatureFileDialog(HWND window, bool saving)
{
    wchar_t filePath[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"Creature Files (*.ctr)\0*.ctr\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = static_cast<DWORD>(std::size(filePath));
    dialog.lpstrDefExt = L"ctr";
    dialog.Flags = saving
        ? (OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT)
        : (OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST);

    const BOOL result = saving ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    return result == TRUE ? std::wstring(filePath) : std::wstring();
}

ShapeModel::Point PointFromLParam(LPARAM lParam)
{
    return ShapeModel::Point{
        static_cast<float>(GET_X_LPARAM(lParam)),
        static_cast<float>(GET_Y_LPARAM(lParam))
    };
}

const wchar_t* ToolName(ShapeModel::ShapeType tool)
{
    switch (tool)
    {
    case ShapeModel::ShapeType::Circle:
        return L"Circle";
    case ShapeModel::ShapeType::Square:
        return L"Square";
    case ShapeModel::ShapeType::Triangle:
        return L"Triangle";
    }

    return L"Unknown";
}

bool ToolFromKey(WPARAM key, ShapeModel::ShapeType& tool)
{
    switch (key)
    {
    case '1':
        tool = ShapeModel::ShapeType::Circle;
        return true;
    case '2':
        tool = ShapeModel::ShapeType::Square;
        return true;
    case '3':
        tool = ShapeModel::ShapeType::Triangle;
        return true;
    default:
        return false;
    }
}

void UpdateWindowTitle(HWND window)
{
    const wchar_t* tool = ToolName(g_drawingState.currentTool);
    const wchar_t* status = ShapeModel::isAnimationPlaying(g_drawingState)
        ? L"Playing"
        : (g_drawingState.shapes.empty() ? L"Start drawing" : L"Ready");
    wchar_t title[300]{};
    wsprintfW(title, L"%s | Tool: %s | %s | 1 Circle, 2 Square, 3 Triangle, P Play/Pause, S Save, L Load, C Clear",
        kWindowTitle,
        tool,
        status);
    SetWindowTextW(window, title);
}

void ShowMessage(HWND window, const wchar_t* title, const std::wstring& message, UINT flags)
{
    MessageBoxW(window, message.c_str(), title, flags | MB_OK);
}

void SaveCreature(HWND window)
{
    const std::wstring filePath = CreatureFileDialog(window, true);
    if (filePath.empty())
    {
        return;
    }

    std::wstring errorMessage;
    if (!ShapeModel::saveCreature(g_drawingState, filePath, errorMessage))
    {
        ShowMessage(window, L"Save Creature", errorMessage, MB_ICONWARNING);
        return;
    }

    ShowMessage(window, L"Save Creature", L"Creature saved successfully.", MB_ICONINFORMATION);
}

void LoadCreature(HWND window)
{
    const std::wstring filePath = CreatureFileDialog(window, false);
    if (filePath.empty())
    {
        return;
    }

    std::wstring errorMessage;
    if (!ShapeModel::loadCreature(g_drawingState, filePath, errorMessage))
    {
        ShowMessage(window, L"Load Creature", errorMessage, MB_ICONWARNING);
        return;
    }

    ShapeModel::cancelDrag(g_drawingState);
    g_isDragging = false;
    UpdateWindowTitle(window);
    InvalidateRect(window, nullptr, FALSE);
}

void RenderWindow(HWND window)
{
    RECT clientRect{};
    GetClientRect(window, &clientRect);
    EngineLoader::RenderFrame(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, g_drawingState);
}
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
            EngineLoader::RenderFrame(width, height, g_drawingState);
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        ShapeModel::ShapeType nextTool = g_drawingState.currentTool;
        if (ToolFromKey(wParam, nextTool))
        {
            g_drawingState.currentTool = nextTool;
            UpdateWindowTitle(window);
            return 0;
        }

        if (wParam == 'C' || wParam == 'c')
        {
            SetAnimationMode(window, false);
            g_drawingState.shapes.clear();
            ShapeModel::cancelDrag(g_drawingState);
            g_isDragging = false;
            UpdateWindowTitle(window);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        if (wParam == 'P' || wParam == 'p')
        {
            if (g_drawingState.shapes.empty())
            {
                ShowMessage(window, L"Animation", L"Draw a connected creature before pressing play.", MB_ICONWARNING);
                return 0;
            }

            if (g_isDragging)
            {
                ShapeModel::cancelDrag(g_drawingState);
                g_isDragging = false;
                ReleaseCapture();
            }

            const bool nextAnimationState = !ShapeModel::isAnimationPlaying(g_drawingState);
            SetAnimationMode(window, nextAnimationState);
            UpdateWindowTitle(window);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        if (wParam == 'S' || wParam == 's')
        {
            SaveCreature(window);
            return 0;
        }

        if (wParam == 'L' || wParam == 'l')
        {
            LoadCreature(window);
            return 0;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
    case WM_TIMER:
    {
        if (wParam != kAnimationTimerId || !ShapeModel::isAnimationPlaying(g_drawingState))
        {
            return 0;
        }

        const DWORD nowMs = GetTickCount();
        DWORD elapsedMs = nowMs - g_lastAnimationTickMs;
        g_lastAnimationTickMs = nowMs;

        elapsedMs = (std::min)(elapsedMs, static_cast<DWORD>(50));
        const float deltaSeconds = static_cast<float>(elapsedMs) * 0.001f;

        RECT clientRect{};
        GetClientRect(window, &clientRect);
        ShapeModel::advanceAnimation(
            g_drawingState,
            deltaSeconds,
            static_cast<float>(clientRect.right - clientRect.left),
            static_cast<float>(clientRect.bottom - clientRect.top));

        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        SetCapture(window);
        g_isDragging = true;
        ShapeModel::beginDrag(g_drawingState, PointFromLParam(lParam));
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        const ShapeModel::Point cursorPoint = PointFromLParam(lParam);
        UpdateAnimationTarget(cursorPoint);

        if (g_isDragging && (wParam & MK_LBUTTON) != 0)
        {
            ShapeModel::updateDrag(g_drawingState, cursorPoint);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        if (g_isDragging)
        {
            ShapeModel::updateDrag(g_drawingState, PointFromLParam(lParam));
            const ShapeModel::CommitResult result = ShapeModel::commitDrag(g_drawingState);
            g_isDragging = false;
            ReleaseCapture();
            if (result == ShapeModel::CommitResult::Detached)
            {
                ShowMessage(window, L"Attach Shapes", L"Each new shape must touch the existing creature.", MB_ICONWARNING);
            }
            UpdateWindowTitle(window);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    case WM_RBUTTONDOWN:
    {
        ShapeModel::cancelDrag(g_drawingState);
        g_isDragging = false;
        ReleaseCapture();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_CAPTURECHANGED:
    {
        if (g_isDragging)
        {
            ShapeModel::cancelDrag(g_drawingState);
            g_isDragging = false;
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);

        RenderWindow(window);

        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        SetAnimationMode(window, false);
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

    g_drawingState.currentStyle.fillColour = ShapeModel::Colour{240, 170, 70, 255};
    g_drawingState.currentStyle.strokeColour = ShapeModel::Colour{245, 245, 245, 255};
    g_drawingState.currentStyle.strokeWidth = 2.0f;
    g_drawingState.currentStyle.hasFill = true;

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    UpdateWindowTitle(window);
    RenderWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    EngineLoader::DestroyOpenGLContext(window);
    return static_cast<int>(message.wParam);
}