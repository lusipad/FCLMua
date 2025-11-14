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
    , m_labelSceneMode(nullptr)
    , m_btnSceneDefault(nullptr)
    , m_btnSceneSolarSystem(nullptr)
    , m_labelSpeed(nullptr)
    , m_btnSpeedPause(nullptr)
    , m_btnSpeed05(nullptr)
    , m_btnSpeed1(nullptr)
    , m_btnSpeed2(nullptr)
    , m_btnSpeed5(nullptr)
    , m_labelAsteroid(nullptr)
    , m_editAsteroidVX(nullptr)
    , m_editAsteroidVY(nullptr)
    , m_editAsteroidVZ(nullptr)
    , m_editAsteroidRadius(nullptr)
    , m_btnCreateAsteroid(nullptr)
    , m_labelAsteroidVelocity(nullptr)
    , m_labelAsteroidRadius(nullptr)
    , m_statusBar(nullptr)
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
    y += spacing + 15;

    // Scene mode section
    m_labelSceneMode = CreateWindowW(
        L"STATIC", L"Scene Mode:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 200, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    y += spacing;

    m_btnSceneDefault = CreateWindowW(
        L"BUTTON", L"Default",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX, y, 60, buttonHeight,
        m_hwnd, (HMENU)3001, m_hInstance, nullptr);

    m_btnSceneSolarSystem = CreateWindowW(
        L"BUTTON", L"Solar",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 65, y, 60, buttonHeight,
        m_hwnd, (HMENU)3002, m_hInstance, nullptr);

    m_btnSceneCrossroad = CreateWindowW(
        L"BUTTON", L"Crossroad",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 130, y, 70, buttonHeight,
        m_hwnd, (HMENU)3003, m_hInstance, nullptr);
    y += spacing + 10;

    // Speed controls section
    m_labelSpeed = CreateWindowW(
        L"STATIC", L"Simulation Speed:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 200, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    y += spacing;

    const int speedBtnWidth = 35;
    m_btnSpeedPause = CreateWindowW(
        L"BUTTON", L"||",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX, y, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4001, m_hInstance, nullptr);

    m_btnSpeed05 = CreateWindowW(
        L"BUTTON", L"0.5x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + speedBtnWidth + 2, y, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4002, m_hInstance, nullptr);

    m_btnSpeed1 = CreateWindowW(
        L"BUTTON", L"1x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + (speedBtnWidth + 2) * 2, y, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4003, m_hInstance, nullptr);

    m_btnSpeed2 = CreateWindowW(
        L"BUTTON", L"2x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + (speedBtnWidth + 2) * 3, y, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4004, m_hInstance, nullptr);

    m_btnSpeed5 = CreateWindowW(
        L"BUTTON", L"5x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + (speedBtnWidth + 2) * 4, y, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4005, m_hInstance, nullptr);
    y += spacing + 10;

    // Asteroid section
    m_labelAsteroid = CreateWindowW(
        L"STATIC", L"Create Asteroid:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 200, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    y += spacing;

    m_labelAsteroidVelocity = CreateWindowW(
        L"STATIC", L"Velocity (X,Y,Z):",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 120, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    y += labelHeight + 5;

    const int smallEditWidth = 45;
    m_editAsteroidVX = CreateWindowW(
        L"EDIT", L"5.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX, y, smallEditWidth, editHeight,
        m_hwnd, (HMENU)1005, m_hInstance, nullptr);

    m_editAsteroidVY = CreateWindowW(
        L"EDIT", L"0.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX + smallEditWidth + 5, y, smallEditWidth, editHeight,
        m_hwnd, (HMENU)1006, m_hInstance, nullptr);

    m_editAsteroidVZ = CreateWindowW(
        L"EDIT", L"0.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX + (smallEditWidth + 5) * 2, y, smallEditWidth, editHeight,
        m_hwnd, (HMENU)1007, m_hInstance, nullptr);
    y += editHeight + 10;

    m_labelAsteroidRadius = CreateWindowW(
        L"STATIC", L"Radius:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);

    m_editAsteroidRadius = CreateWindowW(
        L"EDIT", L"0.3",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX + 65, y, smallEditWidth, editHeight,
        m_hwnd, (HMENU)1008, m_hInstance, nullptr);
    y += editHeight + 5;

    m_btnCreateAsteroid = CreateWindowW(
        L"BUTTON", L"Create Asteroid",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX, y, 180, buttonHeight,
        m_hwnd, (HMENU)5001, m_hInstance, nullptr);
    y += spacing + 15;

    // Vehicle section (for crossroad scene)
    m_labelVehicle = CreateWindowW(
        L"STATIC", L"Create Vehicle:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 200, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    y += spacing;

    m_labelVehicleType = CreateWindowW(
        L"STATIC", L"Type:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);

    m_comboVehicleType = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        panelX + 65, y, 115, 120,
        m_hwnd, (HMENU)1009, m_hInstance, nullptr);
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"Sedan");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"SUV");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"Truck");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"Bus");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"SportsCar");
    SendMessageW(m_comboVehicleType, CB_SETCURSEL, 0, 0);
    y += spacing;

    m_labelVehicleDirection = CreateWindowW(
        L"STATIC", L"Direction:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);

    m_comboVehicleDirection = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        panelX + 65, y, 115, 100,
        m_hwnd, (HMENU)1010, m_hInstance, nullptr);
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"North");
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"South");
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"East");
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"West");
    SendMessageW(m_comboVehicleDirection, CB_SETCURSEL, 0, 0);
    y += spacing;

    m_labelVehicleIntention = CreateWindowW(
        L"STATIC", L"Action:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);

    m_comboVehicleIntention = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        panelX + 65, y, 115, 90,
        m_hwnd, (HMENU)1011, m_hInstance, nullptr);
    SendMessageW(m_comboVehicleIntention, CB_ADDSTRING, 0, (LPARAM)L"Straight");
    SendMessageW(m_comboVehicleIntention, CB_ADDSTRING, 0, (LPARAM)L"Turn Left");
    SendMessageW(m_comboVehicleIntention, CB_ADDSTRING, 0, (LPARAM)L"Turn Right");
    SendMessageW(m_comboVehicleIntention, CB_SETCURSEL, 0, 0);
    y += spacing;

    m_labelVehicleSpeed = CreateWindowW(
        L"STATIC", L"Speed:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);

    m_editVehicleSpeed = CreateWindowW(
        L"EDIT", L"3.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX + 65, y, 60, editHeight,
        m_hwnd, (HMENU)1012, m_hInstance, nullptr);
    y += editHeight + 5;

    m_btnCreateVehicle = CreateWindowW(
        L"BUTTON", L"Add Vehicle",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX, y, 180, buttonHeight,
        m_hwnd, (HMENU)6001, m_hInstance, nullptr);
    y += spacing + 10;

    // OBJ loading section
    m_labelOBJScale = CreateWindowW(
        L"STATIC", L"OBJ Scale:",
        WS_CHILD | WS_VISIBLE,
        panelX, y, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);

    m_editOBJScale = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        panelX + 65, y, 60, editHeight,
        m_hwnd, (HMENU)1013, m_hInstance, nullptr);
    y += editHeight + 5;

    m_btnLoadOBJ = CreateWindowW(
        L"BUTTON", L"Load OBJ Model...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX, y, 180, buttonHeight,
        m_hwnd, (HMENU)6002, m_hInstance, nullptr);

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

    SendMessage(m_labelSceneMode, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSceneDefault, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSceneSolarSystem, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSceneCrossroad, WM_SETFONT, (WPARAM)hFont, TRUE);

    SendMessage(m_labelSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSpeedPause, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSpeed05, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSpeed1, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSpeed2, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnSpeed5, WM_SETFONT, (WPARAM)hFont, TRUE);

    SendMessage(m_labelAsteroid, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelAsteroidVelocity, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editAsteroidVX, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editAsteroidVY, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editAsteroidVZ, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelAsteroidRadius, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editAsteroidRadius, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnCreateAsteroid, WM_SETFONT, (WPARAM)hFont, TRUE);

    SendMessage(m_labelVehicle, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelVehicleType, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_comboVehicleType, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelVehicleDirection, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_comboVehicleDirection, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelVehicleIntention, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_comboVehicleIntention, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelVehicleSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editVehicleSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnCreateVehicle, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelOBJScale, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editOBJScale, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_btnLoadOBJ, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Create status bar at bottom of window
    m_statusBar = CreateWindowExW(
        0,
        L"STATIC",
        L"Ready | Right-click: Rotate | Middle-click: Pan | Scroll: Zoom | Left-click: Drag object",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, m_height - 25, m_width, 25,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_statusBar, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Add colored background for status bar
    HBRUSH statusBrush = CreateSolidBrush(RGB(240, 240, 240));
    SetClassLongPtr(m_statusBar, GCLP_HBRBACKGROUND, (LONG_PTR)statusBrush);
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
        case 3001: // Scene mode: Default
        {
            if (OnSceneModeChanged)
                OnSceneModeChanged(0);
            break;
        }
        case 3002: // Scene mode: Solar System
        {
            if (OnSceneModeChanged)
                OnSceneModeChanged(1);
            break;
        }
        case 3003: // Scene mode: Crossroad
        {
            if (OnSceneModeChanged)
                OnSceneModeChanged(2);
            break;
        }
        case 4001: // Speed: Pause
        {
            if (OnSimulationSpeedChanged)
                OnSimulationSpeedChanged(0.0f);
            break;
        }
        case 4002: // Speed: 0.5x
        {
            if (OnSimulationSpeedChanged)
                OnSimulationSpeedChanged(0.5f);
            break;
        }
        case 4003: // Speed: 1x
        {
            if (OnSimulationSpeedChanged)
                OnSimulationSpeedChanged(1.0f);
            break;
        }
        case 4004: // Speed: 2x
        {
            if (OnSimulationSpeedChanged)
                OnSimulationSpeedChanged(2.0f);
            break;
        }
        case 4005: // Speed: 5x
        {
            if (OnSimulationSpeedChanged)
                OnSimulationSpeedChanged(5.0f);
            break;
        }
        case 5001: // Create Asteroid button
        {
            wchar_t bufferVX[32], bufferVY[32], bufferVZ[32], bufferRadius[32];
            GetWindowTextW(m_editAsteroidVX, bufferVX, 32);
            GetWindowTextW(m_editAsteroidVY, bufferVY, 32);
            GetWindowTextW(m_editAsteroidVZ, bufferVZ, 32);
            GetWindowTextW(m_editAsteroidRadius, bufferRadius, 32);
            float vx = static_cast<float>(_wtof(bufferVX));
            float vy = static_cast<float>(_wtof(bufferVY));
            float vz = static_cast<float>(_wtof(bufferVZ));
            float radius = static_cast<float>(_wtof(bufferRadius));
            if (radius <= 0.0f) radius = 0.3f;
            if (OnCreateAsteroid)
                OnCreateAsteroid(vx, vy, vz, radius);
            break;
        }
        case 6001: // Create Vehicle button
        {
            // Get vehicle type
            int vehicleType = static_cast<int>(SendMessageW(m_comboVehicleType, CB_GETCURSEL, 0, 0));
            // Get direction
            int direction = static_cast<int>(SendMessageW(m_comboVehicleDirection, CB_GETCURSEL, 0, 0));
            // Get movement intention
            int intention = static_cast<int>(SendMessageW(m_comboVehicleIntention, CB_GETCURSEL, 0, 0));
            // Get speed
            wchar_t bufferSpeed[32];
            GetWindowTextW(m_editVehicleSpeed, bufferSpeed, 32);
            float speed = static_cast<float>(_wtof(bufferSpeed));
            if (speed <= 0.0f) speed = 3.0f;

            if (OnCreateVehicle)
                OnCreateVehicle(vehicleType, direction, intention, speed);
            break;
        }
        case 6002: // Load OBJ Model button
        {
            // Open file dialog
            wchar_t filename[MAX_PATH] = L"";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = m_hwnd;
            ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = L"obj";

            if (GetOpenFileNameW(&ofn))
            {
                // Get direction, intention, and speed from current UI state
                int direction = static_cast<int>(SendMessageW(m_comboVehicleDirection, CB_GETCURSEL, 0, 0));
                int intention = static_cast<int>(SendMessageW(m_comboVehicleIntention, CB_GETCURSEL, 0, 0));

                wchar_t bufferSpeed[32];
                GetWindowTextW(m_editVehicleSpeed, bufferSpeed, 32);
                float speed = static_cast<float>(_wtof(bufferSpeed));
                if (speed <= 0.0f) speed = 3.0f;

                wchar_t bufferScale[32];
                GetWindowTextW(m_editOBJScale, bufferScale, 32);
                float scale = static_cast<float>(_wtof(bufferScale));
                if (scale <= 0.0f) scale = 1.0f;

                // Convert wchar_t* to std::string
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL);
                std::string objPath(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, filename, -1, &objPath[0], size_needed, NULL, NULL);

                if (OnLoadVehicleFromOBJ)
                    OnLoadVehicleFromOBJ(objPath, direction, intention, speed, scale);
            }
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
        // Resize status bar to fit window width
        if (m_statusBar)
        {
            SetWindowPos(m_statusBar, nullptr, 0, m_height - 25, m_width, 25, SWP_NOZORDER);
        }
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

        // Accumulate delta for smooth tracking
        m_mouseDX += newMouseX - m_lastMouseX;
        m_mouseDY += newMouseY - m_lastMouseY;

        m_mouseX = newMouseX;
        m_mouseY = newMouseY;
        m_lastMouseX = newMouseX;
        m_lastMouseY = newMouseY;
        return 0;
    }

    case WM_MOUSEWHEEL:
        // Accumulate wheel delta for smooth scrolling
        m_mouseWheel += GET_WHEEL_DELTA_WPARAM(wParam);
        return 0;
    }

    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void Window::SetStatusText(const std::wstring& text)
{
    if (m_statusBar)
    {
        SetWindowTextW(m_statusBar, text.c_str());
    }
}
