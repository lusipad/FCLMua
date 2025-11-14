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
    , m_btnCreateSphere(nullptr)
    , m_btnCreateBox(nullptr)
    , m_btnDelete(nullptr)
    , m_editSphereRadius(nullptr)
    , m_editBoxX(nullptr)
    , m_editBoxY(nullptr)
    , m_editBoxZ(nullptr)
    , m_labelSphereRadius(nullptr)
    , m_labelBox(nullptr)
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

    ATOM result = RegisterClassExW(&wc);
    if (!result)
    {
        DWORD error = GetLastError();
        // CLASS_ALREADY_EXISTS (1410) is OK - window class was already registered
        if (error != ERROR_CLASS_ALREADY_EXISTS)
        {
            wchar_t errorMsg[256];
            swprintf_s(errorMsg, L"Failed to register window class. Error code: 0x%08X", error);
            MessageBoxW(nullptr, errorMsg, L"Window Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    // Calculate window size
    RECT rc = { 0, 0, m_width, m_height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    // Create window
    m_hwnd = CreateWindowExW(
        0,
        L"FclDemoWindowClass",
        m_title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr,
        nullptr,
        m_hInstance,
        this
    );

    if (!m_hwnd)
    {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to create window. Error code: 0x%08X", error);
        MessageBoxW(nullptr, errorMsg, L"Window Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // Create UI controls
    CreateUIControls();

    return true;
}

void Window::CreateUIControls()
{
    const int panelX = 10;
    int y = 10;
    const int spacing = 30;
    const int labelHeight = 20;
    const int buttonHeight = 30;
    const int editHeight = 25;
    const int editWidth = 60;

    // Sphere section
    m_labelSphereRadius = CreateWindowW(
        L"STATIC", L"Create Sphere - Radius:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 200, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    y += spacing;

    m_editSphereRadius = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX, y, editWidth, editHeight,
        m_hwnd, (HMENU)1001, m_hInstance, nullptr);

    m_btnCreateSphere = CreateWindowW(
        L"BUTTON", L"Create Sphere",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + editWidth + 10, y, 100, buttonHeight,
        m_hwnd, (HMENU)2001, m_hInstance, nullptr);
    y += spacing + 10;

    // Box section
    m_labelBox = CreateWindowW(
        L"STATIC", L"Create Box - X, Y, Z:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 200, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    y += spacing;

    m_editBoxX = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX, y, editWidth, editHeight,
        m_hwnd, (HMENU)1002, m_hInstance, nullptr);

    m_editBoxY = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX + editWidth + 5, y, editWidth, editHeight,
        m_hwnd, (HMENU)1003, m_hInstance, nullptr);

    m_editBoxZ = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX + (editWidth + 5) * 2, y, editWidth, editHeight,
        m_hwnd, (HMENU)1004, m_hInstance, nullptr);
    y += editHeight + 5;

    m_btnCreateBox = CreateWindowW(
        L"BUTTON", L"Create Box",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX, y, 180, buttonHeight,
        m_hwnd, (HMENU)2002, m_hInstance, nullptr);
    y += spacing + 10;

    // Delete button
    m_btnDelete = CreateWindowW(
        L"BUTTON", L"Delete Selected",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX, y, 180, buttonHeight,
        m_hwnd, (HMENU)2003, m_hInstance, nullptr);

    // Set font for all controls
    HFONT hFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Microsoft YaHei");

    SendMessage(m_labelSphereRadius, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editSphereRadius, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnCreateSphere, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editBoxX, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editBoxY, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editBoxZ, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnCreateBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnDelete, WM_SETFONT, (WPARAM)hFont, TRUE);
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
        window->m_hwnd = hwnd;  // Set hwnd early
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
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case 2001: // Create Sphere button
        {
            wchar_t buffer[32];
            GetWindowTextW(m_editSphereRadius, buffer, 32);
            float radius = static_cast<float>(_wtof(buffer));
            if (radius <= 0.0f) radius = 1.0f;
            if (OnCreateSphere)
                OnCreateSphere(radius);
            break;
        }
        case 2002: // Create Box button
        {
            wchar_t bufferX[32], bufferY[32], bufferZ[32];
            GetWindowTextW(m_editBoxX, bufferX, 32);
            GetWindowTextW(m_editBoxY, bufferY, 32);
            GetWindowTextW(m_editBoxZ, bufferZ, 32);
            float x = static_cast<float>(_wtof(bufferX));
            float y = static_cast<float>(_wtof(bufferY));
            float z = static_cast<float>(_wtof(bufferZ));
            if (x <= 0.0f) x = 1.0f;
            if (y <= 0.0f) y = 1.0f;
            if (z <= 0.0f) z = 1.0f;
            if (OnCreateBox)
                OnCreateBox(x, y, z);
            break;
        }
        case 2003: // Delete button
        {
            if (OnDeleteObject)
                OnDeleteObject();
            break;
        }
        }
        return 0;
    }

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
