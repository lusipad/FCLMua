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
    , m_labelPerformance(nullptr)
    , m_btnPerfHigh(nullptr)
    , m_btnPerfMedium(nullptr)
    , m_btnPerfLow(nullptr)
    , m_labelAsteroid(nullptr)
    , m_editAsteroidVX(nullptr)
    , m_editAsteroidVY(nullptr)
    , m_editAsteroidVZ(nullptr)
    , m_editAsteroidRadius(nullptr)
    , m_btnCreateAsteroid(nullptr)
    , m_labelAsteroidVelocity(nullptr)
    , m_labelAsteroidRadius(nullptr)
    , m_statusBar(nullptr)
    , m_labelProperties(nullptr)
    , m_labelObjectName(nullptr)
    , m_labelPosX(nullptr), m_labelPosY(nullptr), m_labelPosZ(nullptr)
    , m_editPosX(nullptr), m_editPosY(nullptr), m_editPosZ(nullptr)
    , m_labelRotY(nullptr)
    , m_editRotY(nullptr)
    , m_overlayLabel(nullptr)
    , m_statusPanel(nullptr)
    , m_statusPanelBackground(nullptr)
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
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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
    const int panelWidth = 220;
    int y = 10;
    const int spacing = 28;
    const int labelHeight = 20;
    const int buttonHeight = 28;
    const int editHeight = 24;
    const int editWidth = 60;
    const int groupSpacing = 12;
    const int groupTitleHeight = 28;  // Space for group box title
    const int groupPadding = 10;      // Padding inside group box

    // Create font for all controls
    HFONT hFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    // ===== BASIC OBJECTS GROUP =====
    HWND groupBasic = CreateWindowW(
        L"BUTTON", L"Basic Objects",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        panelX, y, panelWidth, 155,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(groupBasic, WM_SETFONT, (WPARAM)hFont, TRUE);

    int groupY = y + groupTitleHeight;

    // Sphere section
    m_labelSphereRadius = CreateWindowW(
        L"STATIC", L"Sphere Radius:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 90, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelSphereRadius, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    m_editSphereRadius = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 10, groupY, 50, editHeight,
        m_hwnd, (HMENU)1001, m_hInstance, nullptr);
    SendMessage(m_editSphereRadius, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnCreateSphere = CreateWindowW(
        L"BUTTON", L"Create",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 70, groupY, 60, buttonHeight,
        m_hwnd, (HMENU)2001, m_hInstance, nullptr);
    SendMessage(m_btnCreateSphere, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnDelete = CreateWindowW(
        L"BUTTON", L"Delete",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 140, groupY, 70, buttonHeight,
        m_hwnd, (HMENU)2003, m_hInstance, nullptr);
    SendMessage(m_btnDelete, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing + 5;

    // Box section
    m_labelBox = CreateWindowW(
        L"STATIC", L"Box Size (X,Y,Z):",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 120, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    const int smallEditWidth = 45;
    m_editBoxX = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 10, groupY, smallEditWidth, editHeight,
        m_hwnd, (HMENU)1002, m_hInstance, nullptr);
    SendMessage(m_editBoxX, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editBoxY = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 10 + smallEditWidth + 3, groupY, smallEditWidth, editHeight,
        m_hwnd, (HMENU)1003, m_hInstance, nullptr);
    SendMessage(m_editBoxY, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editBoxZ = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 10 + (smallEditWidth + 3) * 2, groupY, smallEditWidth, editHeight,
        m_hwnd, (HMENU)1004, m_hInstance, nullptr);
    SendMessage(m_editBoxZ, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnCreateBox = CreateWindowW(
        L"BUTTON", L"Create",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 155, groupY - 1, 55, buttonHeight,
        m_hwnd, (HMENU)2002, m_hInstance, nullptr);
    SendMessage(m_btnCreateBox, WM_SETFONT, (WPARAM)hFont, TRUE);

    y = groupY + buttonHeight + groupSpacing + groupPadding;

    // ===== SCENE MODE GROUP =====
    HWND groupScene = CreateWindowW(
        L"BUTTON", L"Scene Mode",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        panelX, y, panelWidth, 110,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(groupScene, WM_SETFONT, (WPARAM)hFont, TRUE);

    groupY = y + groupTitleHeight;

    m_labelSceneMode = CreateWindowW(
        L"STATIC", L"Select scene:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 100, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelSceneMode, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    m_btnSceneDefault = CreateWindowW(
        L"BUTTON", L"Default",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10, groupY, 63, buttonHeight,
        m_hwnd, (HMENU)3001, m_hInstance, nullptr);
    SendMessage(m_btnSceneDefault, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnSceneSolarSystem = CreateWindowW(
        L"BUTTON", L"Solar",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 78, groupY, 63, buttonHeight,
        m_hwnd, (HMENU)3002, m_hInstance, nullptr);
    SendMessage(m_btnSceneSolarSystem, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnSceneCrossroad = CreateWindowW(
        L"BUTTON", L"Crossroad",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 146, groupY, 64, buttonHeight,
        m_hwnd, (HMENU)3003, m_hInstance, nullptr);
    SendMessage(m_btnSceneCrossroad, WM_SETFONT, (WPARAM)hFont, TRUE);

    y = groupY + buttonHeight + groupSpacing + groupPadding;

    // ===== SIMULATION SPEED GROUP =====
    HWND groupSpeed = CreateWindowW(
        L"BUTTON", L"Simulation Speed",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        panelX, y, panelWidth, 95,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(groupSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);

    groupY = y + groupTitleHeight;

    m_labelSpeed = CreateWindowW(
        L"STATIC", L"Control:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    const int speedBtnWidth = 38;
    m_btnSpeedPause = CreateWindowW(
        L"BUTTON", L"||",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10, groupY, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4001, m_hInstance, nullptr);
    SendMessage(m_btnSpeedPause, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnSpeed05 = CreateWindowW(
        L"BUTTON", L".5x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10 + speedBtnWidth + 2, groupY, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4002, m_hInstance, nullptr);
    SendMessage(m_btnSpeed05, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnSpeed1 = CreateWindowW(
        L"BUTTON", L"1x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10 + (speedBtnWidth + 2) * 2, groupY, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4003, m_hInstance, nullptr);
    SendMessage(m_btnSpeed1, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnSpeed2 = CreateWindowW(
        L"BUTTON", L"2x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10 + (speedBtnWidth + 2) * 3, groupY, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4004, m_hInstance, nullptr);
    SendMessage(m_btnSpeed2, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnSpeed5 = CreateWindowW(
        L"BUTTON", L"5x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10 + (speedBtnWidth + 2) * 4, groupY, speedBtnWidth, buttonHeight,
        m_hwnd, (HMENU)4005, m_hInstance, nullptr);
    SendMessage(m_btnSpeed5, WM_SETFONT, (WPARAM)hFont, TRUE);

    y = groupY + buttonHeight + groupSpacing + groupPadding;

    // ===== PERFORMANCE MODE GROUP =====
    HWND groupPerformance = CreateWindowW(
        L"BUTTON", L"Performance Mode",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        panelX, y, panelWidth, 95,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(groupPerformance, WM_SETFONT, (WPARAM)hFont, TRUE);

    groupY = y + groupTitleHeight;

    m_labelPerformance = CreateWindowW(
        L"STATIC", L"Collision Det:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 90, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelPerformance, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    const int perfBtnWidth = 55;
    m_btnPerfHigh = CreateWindowW(
        L"BUTTON", L"High",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10, groupY, perfBtnWidth, buttonHeight,
        m_hwnd, (HMENU)5001, m_hInstance, nullptr);
    SendMessage(m_btnPerfHigh, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnPerfMedium = CreateWindowW(
        L"BUTTON", L"Medium",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10 + perfBtnWidth + 2, groupY, perfBtnWidth, buttonHeight,
        m_hwnd, (HMENU)5002, m_hInstance, nullptr);
    SendMessage(m_btnPerfMedium, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnPerfLow = CreateWindowW(
        L"BUTTON", L"Low (VM)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10 + (perfBtnWidth + 2) * 2, groupY, perfBtnWidth, buttonHeight,
        m_hwnd, (HMENU)5003, m_hInstance, nullptr);
    SendMessage(m_btnPerfLow, WM_SETFONT, (WPARAM)hFont, TRUE);

    y = groupY + buttonHeight + groupSpacing + groupPadding;

    // ===== ASTEROID GROUP (Solar System Scene) =====
    HWND groupAsteroid = CreateWindowW(
        L"BUTTON", L"Asteroid (Solar Scene)",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        panelX, y, panelWidth, 150,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(groupAsteroid, WM_SETFONT, (WPARAM)hFont, TRUE);

    groupY = y + groupTitleHeight;

    m_labelAsteroid = CreateWindowW(
        L"STATIC", L"Velocity (X,Y,Z):",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 120, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelAsteroid, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    const int vEditWidth = 45;
    m_editAsteroidVX = CreateWindowW(
        L"EDIT", L"5.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 10, groupY, vEditWidth, editHeight,
        m_hwnd, (HMENU)1005, m_hInstance, nullptr);
    SendMessage(m_editAsteroidVX, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editAsteroidVY = CreateWindowW(
        L"EDIT", L"0.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 10 + vEditWidth + 3, groupY, vEditWidth, editHeight,
        m_hwnd, (HMENU)1006, m_hInstance, nullptr);
    SendMessage(m_editAsteroidVY, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editAsteroidVZ = CreateWindowW(
        L"EDIT", L"0.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 10 + (vEditWidth + 3) * 2, groupY, vEditWidth, editHeight,
        m_hwnd, (HMENU)1007, m_hInstance, nullptr);
    SendMessage(m_editAsteroidVZ, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += editHeight + 8;

    m_labelAsteroidRadius = CreateWindowW(
        L"STATIC", L"Radius:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelAsteroidRadius, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editAsteroidRadius = CreateWindowW(
        L"EDIT", L"0.3",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 75, groupY, 50, editHeight,
        m_hwnd, (HMENU)1008, m_hInstance, nullptr);
    SendMessage(m_editAsteroidRadius, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += editHeight + 8;

    m_btnCreateAsteroid = CreateWindowW(
        L"BUTTON", L"Create Asteroid",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10, groupY, 200, buttonHeight,
        m_hwnd, (HMENU)6001, m_hInstance, nullptr);
    SendMessage(m_btnCreateAsteroid, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_labelAsteroidVelocity = m_labelAsteroid;  // Reuse for compatibility
    y = groupY + buttonHeight + groupSpacing + groupPadding;

    // ===== VEHICLE GROUP (Crossroad Scene) =====
    HWND groupVehicle = CreateWindowW(
        L"BUTTON", L"Vehicle (Crossroad Scene)",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        panelX, y, panelWidth, 210,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(groupVehicle, WM_SETFONT, (WPARAM)hFont, TRUE);

    groupY = y + groupTitleHeight;

    m_labelVehicle = CreateWindowW(
        L"STATIC", L"Type:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 50, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelVehicle, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_comboVehicleType = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        panelX + 65, groupY - 2, 145, 120,
        m_hwnd, (HMENU)1009, m_hInstance, nullptr);
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"Sedan");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"SUV");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"Truck");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"Bus");
    SendMessageW(m_comboVehicleType, CB_ADDSTRING, 0, (LPARAM)L"SportsCar");
    SendMessageW(m_comboVehicleType, CB_SETCURSEL, 0, 0);
    SendMessage(m_comboVehicleType, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    m_labelVehicleType = m_labelVehicle;  // Reuse for compatibility

    m_labelVehicleDirection = CreateWindowW(
        L"STATIC", L"From:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 50, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelVehicleDirection, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_comboVehicleDirection = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        panelX + 65, groupY - 2, 145, 100,
        m_hwnd, (HMENU)1010, m_hInstance, nullptr);
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"North");
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"South");
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"East");
    SendMessageW(m_comboVehicleDirection, CB_ADDSTRING, 0, (LPARAM)L"West");
    SendMessageW(m_comboVehicleDirection, CB_SETCURSEL, 0, 0);
    SendMessage(m_comboVehicleDirection, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    m_labelVehicleIntention = CreateWindowW(
        L"STATIC", L"Action:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 50, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelVehicleIntention, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_comboVehicleIntention = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        panelX + 65, groupY - 2, 145, 90,
        m_hwnd, (HMENU)1011, m_hInstance, nullptr);
    SendMessageW(m_comboVehicleIntention, CB_ADDSTRING, 0, (LPARAM)L"Straight");
    SendMessageW(m_comboVehicleIntention, CB_ADDSTRING, 0, (LPARAM)L"Turn Left");
    SendMessageW(m_comboVehicleIntention, CB_ADDSTRING, 0, (LPARAM)L"Turn Right");
    SendMessageW(m_comboVehicleIntention, CB_SETCURSEL, 0, 0);
    SendMessage(m_comboVehicleIntention, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    m_labelVehicleSpeed = CreateWindowW(
        L"STATIC", L"Speed:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 50, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelVehicleSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editVehicleSpeed = CreateWindowW(
        L"EDIT", L"3.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 65, groupY, 60, editHeight,
        m_hwnd, (HMENU)1012, m_hInstance, nullptr);
    SendMessage(m_editVehicleSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_labelOBJScale = CreateWindowW(
        L"STATIC", L"Scale:",
        WS_CHILD | WS_VISIBLE,
        panelX + 135, groupY, 40, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelOBJScale, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editOBJScale = CreateWindowW(
        L"EDIT", L"1.0",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        panelX + 175, groupY, 35, editHeight,
        m_hwnd, (HMENU)1013, m_hInstance, nullptr);
    SendMessage(m_editOBJScale, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += editHeight + 8;

    m_btnCreateVehicle = CreateWindowW(
        L"BUTTON", L"Add Vehicle",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 10, groupY, 95, buttonHeight,
        m_hwnd, (HMENU)7001, m_hInstance, nullptr);
    SendMessage(m_btnCreateVehicle, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_btnLoadOBJ = CreateWindowW(
        L"BUTTON", L"Load OBJ...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        panelX + 110, groupY, 100, buttonHeight,
        m_hwnd, (HMENU)7002, m_hInstance, nullptr);
    SendMessage(m_btnLoadOBJ, WM_SETFONT, (WPARAM)hFont, TRUE);

    y = groupY + buttonHeight + groupSpacing + groupPadding;

    // ===== PROPERTIES GROUP =====
    HWND groupProps = CreateWindowW(
        L"BUTTON", L"Object Properties",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        panelX, y, panelWidth, 150,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(groupProps, WM_SETFONT, (WPARAM)hFont, TRUE);

    groupY = y + groupTitleHeight;

    m_labelProperties = CreateWindowW(
        L"STATIC", L"Selected:",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 60, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelProperties, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_labelObjectName = CreateWindowW(
        L"STATIC", L"None",
        WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,
        panelX + 75, groupY, 135, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_labelObjectName, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += spacing;

    HWND labelPos = CreateWindowW(
        L"STATIC", L"Position (X,Y,Z):",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 120, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(labelPos, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += labelHeight + 5;

    const int propEditWidth = 52;
    m_editPosX = CreateWindowW(
        L"EDIT", L"0.00",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        panelX + 10, groupY, propEditWidth, editHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_editPosX, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editPosY = CreateWindowW(
        L"EDIT", L"0.00",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        panelX + 10 + propEditWidth + 3, groupY, propEditWidth, editHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_editPosY, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editPosZ = CreateWindowW(
        L"EDIT", L"0.00",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        panelX + 10 + (propEditWidth + 3) * 2, groupY, propEditWidth, editHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_editPosZ, WM_SETFONT, (WPARAM)hFont, TRUE);
    groupY += editHeight + 10;

    HWND labelRot = CreateWindowW(
        L"STATIC", L"Rotation Y (deg):",
        WS_CHILD | WS_VISIBLE,
        panelX + 10, groupY, 100, labelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(labelRot, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_editRotY = CreateWindowW(
        L"EDIT", L"0.00",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER,
        panelX + 115, groupY, 65, editHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_editRotY, WM_SETFONT, (WPARAM)hFont, TRUE);
    m_labelRotY = labelRot;  // Store for compatibility

    y = groupY + editHeight + groupSpacing;

    // Overlay diagnostics label over 3D view
    m_overlayLabel = CreateWindowExW(
        WS_EX_TRANSPARENT,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        panelX + 230, // slightly to the right of panel
        10,
        380,
        60,
        m_hwnd,
        nullptr,
        m_hInstance,
        nullptr);

    SendMessage(m_labelProperties, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_labelObjectName, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editPosX, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editPosY, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editPosZ, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editRotY, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_overlayLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // ===== ENHANCED STATUS PANEL (Semi-transparent, top-right) =====
    // Create background for status panel (semi-transparent)
    const int statusPanelX = m_width - 310;
    const int statusPanelY = 10;
    const int statusPanelWidth = 295;
    const int statusPanelHeight = 140;

    // Create static control with layered window style for transparency effect
    m_statusPanelBackground = CreateWindowExW(
        WS_EX_TRANSPARENT,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        statusPanelX, statusPanelY, statusPanelWidth, statusPanelHeight,
        m_hwnd, nullptr, m_hInstance, nullptr);

    // Create actual status panel text on top
    m_statusPanel = CreateWindowExW(
        WS_EX_TRANSPARENT,
        L"STATIC",
        L"System Status\n"
        L"━━━━━━━━━━━━━━━━━━\n"
        L"FPS: --\n"
        L"Frame Time: -- ms\n"
        L"Objects: 0\n"
        L"Scene: Default\n"
        L"Selected: None",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        statusPanelX + 10, statusPanelY + 8,
        statusPanelWidth - 20, statusPanelHeight - 16,
        m_hwnd, nullptr, m_hInstance, nullptr);

    // Use a monospace font for better alignment
    HFONT hStatusFont = CreateFontW(
        15, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN,
        L"Consolas");
    SendMessage(m_statusPanel, WM_SETFONT, (WPARAM)hStatusFont, TRUE);

    // Set text color to white for better visibility on semi-transparent background
    SetBkMode(GetDC(m_statusPanel), TRANSPARENT);

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
        case 5001: // Performance: High
        {
            if (OnPerformanceModeChanged)
                OnPerformanceModeChanged(0);  // 0 = High
            break;
        }
        case 5002: // Performance: Medium
        {
            if (OnPerformanceModeChanged)
                OnPerformanceModeChanged(1);  // 1 = Medium
            break;
        }
        case 5003: // Performance: Low
        {
            if (OnPerformanceModeChanged)
                OnPerformanceModeChanged(2);  // 2 = Low
            break;
        }
        case 6001: // Create Asteroid button
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
        case 7001: // Create Vehicle button
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
        case 7002: // Load OBJ Model button
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

    case WM_GETMINMAXINFO:
    {
        // Set minimum window size to prevent UI from becoming unusable
        MINMAXINFO* pMinMaxInfo = (MINMAXINFO*)lParam;
        // Keep a reasonably wide viewport for 3D view and ensure
        // left-side control panel has enough vertical space so that
        // controls do not visually overlap when the user resizes.
        pMinMaxInfo->ptMinTrackSize.x = 1200;  // Minimum width
        pMinMaxInfo->ptMinTrackSize.y = 820;   // Minimum height
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

        // Reposition status panel to stay in top-right corner
        if (m_statusPanel && m_statusPanelBackground)
        {
            const int statusPanelWidth = 295;
            const int statusPanelHeight = 140;
            const int statusPanelX = m_width - statusPanelWidth - 10;
            const int statusPanelY = 10;

            // Move background
            SetWindowPos(m_statusPanelBackground, nullptr,
                         statusPanelX, statusPanelY,
                         statusPanelWidth, statusPanelHeight,
                         SWP_NOZORDER);

            // Move text panel
            SetWindowPos(m_statusPanel, nullptr,
                         statusPanelX + 10, statusPanelY + 8,
                         statusPanelWidth - 20, statusPanelHeight - 16,
                         SWP_NOZORDER);
        }

        // Reposition overlay label (collision stats) to stay next to left panel
        if (m_overlayLabel)
        {
            const int overlayX = 240;  // Just to the right of left panel
            const int overlayY = 10;
            const int overlayWidth = 380;
            const int overlayHeight = 60;

            SetWindowPos(m_overlayLabel, nullptr,
                         overlayX, overlayY,
                         overlayWidth, overlayHeight,
                         SWP_NOZORDER);
        }

        return 0;

    case WM_KEYDOWN:
        // Handle F1 key for help
        if (wParam == VK_F1)
        {
            ShowHelpDialog();
            return 0;
        }
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

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;

        // Semi-transparent background for status panel
        if (hwndStatic == m_statusPanelBackground)
        {
            static HBRUSH hbrBkgnd = CreateSolidBrush(RGB(30, 30, 40));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)hbrBkgnd;
        }

        // Status panel text - white text on dark semi-transparent background
        if (hwndStatic == m_statusPanel)
        {
            SetTextColor(hdcStatic, RGB(220, 220, 220));
            SetBkColor(hdcStatic, RGB(30, 30, 40));
            static HBRUSH hbrText = CreateSolidBrush(RGB(30, 30, 40));
            return (LRESULT)hbrText;
        }

        break;
    }
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

void Window::ShowHelpDialog()
{
    const wchar_t* helpText =
        L"FCL Collision Detection Demo - Help\n\n"
        L"CAMERA CONTROLS:\n"
        L"  Right Mouse Button + Drag  - Rotate camera around target\n"
        L"  Middle Mouse Button + Drag - Pan camera (move target)\n"
        L"  Mouse Wheel                - Zoom in/out\n\n"
        L"OBJECT MANIPULATION:\n"
        L"  Left Mouse Button + Drag   - Move selected object on XZ plane\n"
        L"  W / S Keys                 - Move selected object up/down\n"
        L"  Q / E Keys                 - Rotate selected object (Y-axis)\n"
        L"  1-9 Keys                   - Select object by index\n"
        L"  ESC Key                    - Deselect all objects\n"
        L"  Delete Key                 - Delete selected object\n\n"
        L"OBJECT CREATION:\n"
        L"  Ctrl + C                   - Create sphere at camera target\n"
        L"  Ctrl + B                   - Create box at camera target\n"
        L"  UI Buttons                 - Create various objects\n\n"
        L"SCENE MODES:\n"
        L"  Default        - Interactive demo with spheres and boxes\n"
        L"  Solar System   - Planetary orbits simulation\n"
        L"  Crossroad      - Traffic intersection simulation\n\n"
        L"SIMULATION:\n"
        L"  Speed Controls - Adjust simulation speed (0x to 5x)\n"
        L"  Pause Button   - Pause/resume simulation\n\n"
        L"VISUAL FEEDBACK:\n"
        L"  Yellow Object  - Currently selected\n"
        L"  Red Object     - Collision detected\n"
        L"  Colored Axes   - Selection gizmo (X=red, Y=green, Z=blue)\n\n"
        L"Press F1 at any time to show this help.";

    MessageBoxW(m_hwnd, helpText, L"Help - Controls and Features", MB_OK | MB_ICONINFORMATION);
}

void Window::UpdatePropertiesPanel(size_t selectedIndex, const std::string& objectName,
                              float posX, float posY, float posZ,
                              float rotY)
{
    if (!m_labelObjectName) return;

    if (selectedIndex == static_cast<size_t>(-1))
    {
        // No object selected
        SetWindowTextW(m_labelObjectName, L"None");
        SetWindowTextW(m_editPosX, L"-");
        SetWindowTextW(m_editPosY, L"-");
        SetWindowTextW(m_editPosZ, L"-");
        SetWindowTextW(m_editRotY, L"-");
    }
    else
    {
        // Object selected - show properties
        wchar_t buffer[256];

        // Object name
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, objectName.c_str(), -1, NULL, 0);
        std::wstring wObjectName(size_needed - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, objectName.c_str(), -1, &wObjectName[0], size_needed);
        SetWindowTextW(m_labelObjectName, wObjectName.c_str());

        // Position
        swprintf_s(buffer, L"%.2f", posX);
        SetWindowTextW(m_editPosX, buffer);
        swprintf_s(buffer, L"%.2f", posY);
        SetWindowTextW(m_editPosY, buffer);
        swprintf_s(buffer, L"%.2f", posZ);
        SetWindowTextW(m_editPosZ, buffer);

        // Rotation (convert radians to degrees)
        float rotDegrees = rotY * 180.0f / 3.14159265f;
        swprintf_s(buffer, L"%.1f", rotDegrees);
        SetWindowTextW(m_editRotY, buffer);
    }
}

void Window::SetOverlayText(const std::wstring& text)
{
    if (m_overlayLabel && m_hwnd)
    {
        SetWindowTextW(m_overlayLabel, text.c_str());
    }
}

void Window::UpdateStatusPanel(float fps, float frameTime, size_t objectCount,
                               const std::wstring& sceneMode, const std::wstring& selectedObject)
{
    if (!m_statusPanel) return;

    wchar_t statusText[512];
    swprintf_s(statusText,
        L"System Status\n"
        L"━━━━━━━━━━━━━━━━━━\n"
        L"FPS: %.1f\n"
        L"Frame Time: %.2f ms\n"
        L"Objects: %zu\n"
        L"Scene: %s\n"
        L"Selected: %s",
        fps,
        frameTime,
        objectCount,
        sceneMode.c_str(),
        selectedObject.c_str());

    SetWindowTextW(m_statusPanel, statusText);
}
