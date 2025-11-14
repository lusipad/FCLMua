#include "window.h"
#include <windowsx.h>

Window::Window(HINSTANCE hInstance, const std::wstring& title, int width, int height)
    : m_hInstance(hInstance)
    , m_hwnd(nullptr)
    , m_title(title)
    , m_width(width)
    , m_height(height)
    , m_mouseX(0)
    , m_mouseY(0)
    , m_mouseDX(0)
    , m_mouseDY(0)
    , m_lastMouseX(0)
    , m_lastMouseY(0)
    , m_mouseWheel(0)
{
    ZeroMemory(m_keys, sizeof(m_keys));
    ZeroMemory(m_mouseButtons, sizeof(m_mouseButtons));
}

Window::~Window()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
    }
}

bool Window::Initialize()
{
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FclDemoWindowClass";

    if (!RegisterClassExW(&wc))
    {
        return false;
    }

    // Calculate window size
    RECT rc = { 0, 0, m_width, m_height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    // Create window
    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        m_title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr,
        nullptr,
        m_hInstance,
        this
    );

    return m_hwnd != nullptr;
}

void Window::Show(int nCmdShow)
{
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Window* window = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    else
    {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window)
    {
        return window->HandleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);
        return 0;

    case WM_KEYDOWN:
        if (wParam < 256)
            m_keys[wParam] = true;
        return 0;

    case WM_KEYUP:
        if (wParam < 256)
            m_keys[wParam] = false;
        return 0;

    case WM_LBUTTONDOWN:
        m_mouseButtons[0] = true;
        SetCapture(m_hwnd);
        return 0;

    case WM_LBUTTONUP:
        m_mouseButtons[0] = false;
        ReleaseCapture();
        return 0;

    case WM_RBUTTONDOWN:
        m_mouseButtons[1] = true;
        SetCapture(m_hwnd);
        return 0;

    case WM_RBUTTONUP:
        m_mouseButtons[1] = false;
        ReleaseCapture();
        return 0;

    case WM_MBUTTONDOWN:
        m_mouseButtons[2] = true;
        SetCapture(m_hwnd);
        return 0;

    case WM_MBUTTONUP:
        m_mouseButtons[2] = false;
        ReleaseCapture();
        return 0;

    case WM_MOUSEMOVE:
    {
        int newMouseX = GET_X_LPARAM(lParam);
        int newMouseY = GET_Y_LPARAM(lParam);

        m_mouseDX = newMouseX - m_lastMouseX;
        m_mouseDY = newMouseY - m_lastMouseY;

        m_mouseX = newMouseX;
        m_mouseY = newMouseY;
        m_lastMouseX = newMouseX;
        m_lastMouseY = newMouseY;
        return 0;
    }

    case WM_MOUSEWHEEL:
        m_mouseWheel = GET_WHEEL_DELTA_WPARAM(wParam);
        return 0;
    }

    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}
