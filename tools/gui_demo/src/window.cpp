#include "window.h"
#include <windowsx.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <vector>
#include <string>
#include <sstream>

#pragma comment(lib, "uxtheme.lib")

// Control IDs
#define ID_EDIT_SPHERE_RADIUS 1001
#define ID_EDIT_BOX_X 1002
#define ID_EDIT_BOX_Y 1003
#define ID_EDIT_BOX_Z 1004
#define ID_EDIT_ASTEROID_VX 1005
#define ID_EDIT_ASTEROID_VY 1006
#define ID_EDIT_ASTEROID_VZ 1007
#define ID_EDIT_ASTEROID_RADIUS 1008
#define ID_COMBO_VEHICLE_TYPE 1009
#define ID_COMBO_VEHICLE_DIR 1010
#define ID_COMBO_VEHICLE_INTENT 1011
#define ID_EDIT_VEHICLE_SPEED 1012
#define ID_EDIT_OBJ_SCALE 1013

#define ID_BTN_CREATE_SPHERE 2001
#define ID_BTN_CREATE_BOX 2002
#define ID_BTN_DELETE 2003

#define ID_BTN_SCENE_DEFAULT 3001
#define ID_BTN_SCENE_SOLAR 3002
#define ID_BTN_SCENE_CROSSROAD 3003

#define ID_BTN_SPEED_PAUSE 4001
#define ID_BTN_SPEED_05 4002
#define ID_BTN_SPEED_1 4003
#define ID_BTN_SPEED_2 4004
#define ID_BTN_SPEED_5 4005

#define ID_BTN_PERF_HIGH 5001
#define ID_BTN_PERF_MEDIUM 5002
#define ID_BTN_PERF_LOW 5003

#define ID_BTN_CREATE_ASTEROID 6001
#define ID_BTN_CREATE_VEHICLE 7001
#define ID_BTN_LOAD_OBJ 7002

Window::Window(HINSTANCE hInstance, const std::wstring& title, int width, int height)
    : m_hInstance(hInstance)
    , m_hwnd(nullptr)
    , m_title(title)
    , m_width(width)
    , m_height(height)
    , m_mouseX(0), m_mouseY(0), m_mouseDX(0), m_mouseDY(0)
    , m_lastMouseX(0), m_lastMouseY(0), m_mouseWheel(0)
    , m_hoveredButton(nullptr)
{
    ZeroMemory(m_keys, sizeof(m_keys));
    ZeroMemory(m_mouseButtons, sizeof(m_mouseButtons));

    // Initialize theme resources
    m_hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    m_hbrPanel = CreateSolidBrush(RGB(45, 45, 48));
    m_hbrEdit = CreateSolidBrush(RGB(60, 60, 60));
    m_hbrButton = CreateSolidBrush(RGB(60, 60, 60));
    m_hbrButtonHover = CreateSolidBrush(RGB(80, 80, 80));
    m_hbrButtonActive = CreateSolidBrush(RGB(100, 100, 100));
    m_hbrAccent = CreateSolidBrush(RGB(0, 120, 215));
    
    m_textColor = RGB(220, 220, 220);
    m_accentColor = RGB(0, 120, 215);

    // Create fonts
    m_hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        
    m_hFontBold = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_hFontSmall = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

Window::~Window()
{
    DeleteObject(m_hbrBackground);
    DeleteObject(m_hbrPanel);
    DeleteObject(m_hbrEdit);
    DeleteObject(m_hbrButton);
    DeleteObject(m_hbrButtonHover);
    DeleteObject(m_hbrButtonActive);
    DeleteObject(m_hbrAccent);
    DeleteObject(m_hFont);
    DeleteObject(m_hFontBold);
    DeleteObject(m_hFontSmall);

    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool Window::Initialize()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_hbrBackground;
    wc.lpszClassName = L"FclDemoWindowClass";

    if (!RegisterClassExW(&wc))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    }

    // Register Scrollable Panel Class
    WNDCLASSEXW wcPanel = {};
    wcPanel.cbSize = sizeof(WNDCLASSEXW);
    wcPanel.style = CS_HREDRAW | CS_VREDRAW;
    wcPanel.lpfnWndProc = PanelProc; // Use the new static proc
    wcPanel.hInstance = m_hInstance;
    wcPanel.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcPanel.hbrBackground = m_hbrBackground; // Match theme
    wcPanel.lpszClassName = L"FclDemoPanelClass";

    if (!RegisterClassExW(&wcPanel))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    }

    RECT rc = { 0, 0, m_width, m_height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowExW(0, L"FclDemoWindowClass", m_title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, // Restore CLIPCHILDREN to protect UI from DX overwrite
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwnd) return false;

    CreateUIControls();
    UpdateLayout();

    return true;
}

void Window::CreateUIControls()
{
    // 1. Create the Panel Container (Clips children, handles scrollbar events)
    // Restore WS_CLIPCHILDREN now that text rendering is fixed (OPAQUE)
    m_panelContainer = CreateWindowW(L"FclDemoPanelClass", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
        0, 0, 300, 600, m_hwnd, nullptr, m_hInstance, this);

    // 2. Create the Panel Content (Holds the actual controls, moves up/down)
    m_panelContent = CreateWindowW(L"FclDemoPanelClass", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 280, 2000, m_panelContainer, nullptr, m_hInstance, this);

    m_scrollOffset = 0;
    m_totalContentHeight = 0;

    // Helper lambda to create controls
    auto CreateLabel = [&](const wchar_t* text, HWND* outHwnd, bool bold = false) {
        *outHwnd = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, m_panelContent, nullptr, m_hInstance, nullptr);
        SendMessage(*outHwnd, WM_SETFONT, (WPARAM)(bold ? m_hFontBold : m_hFont), TRUE);
    };

    auto CreateEdit = [&](const wchar_t* text, int id, HWND* outHwnd, bool readOnly = false) {
        // Remove WS_BORDER for flat style
        DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER; 
        if (readOnly) style |= ES_READONLY;
        *outHwnd = CreateWindowW(L"EDIT", text, style,
            0, 0, 0, 0, m_panelContent, (HMENU)id, m_hInstance, nullptr);
        SendMessage(*outHwnd, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    };

    auto CreateButton = [&](const wchar_t* text, int id, HWND* outHwnd) {
        *outHwnd = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, m_panelContent, (HMENU)id, m_hInstance, nullptr);
        SendMessage(*outHwnd, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    };

    auto CreateCombo = [&](int id, HWND* outHwnd, const std::vector<std::wstring>& items) {
        // Add OwnerDraw styles for dark theme customization
        *outHwnd = CreateWindowW(L"COMBOBOX", L"", 
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
            0, 0, 0, 0, m_panelContent, (HMENU)id, m_hInstance, nullptr);
        SendMessage(*outHwnd, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        for (const auto& item : items)
            SendMessageW(*outHwnd, CB_ADDSTRING, 0, (LPARAM)item.c_str());
        SendMessageW(*outHwnd, CB_SETCURSEL, 0, 0);
    };

    auto CreateDivider = [&]() {
        HWND hDiv = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, m_panelContent, nullptr, m_hInstance, nullptr);
        m_dividers.push_back(hDiv);
    };

    // --- Basic Objects ---
    CreateLabel(L"BASIC OBJECTS", &m_labelBasicGroup, true);
    CreateDivider();
    CreateLabel(L"Sphere Radius:", &m_labelSphereRadius);
    CreateEdit(L"1.0", ID_EDIT_SPHERE_RADIUS, &m_editSphereRadius);
    CreateButton(L"Create Sphere", ID_BTN_CREATE_SPHERE, &m_btnCreateSphere);
    CreateButton(L"Delete Selected", ID_BTN_DELETE, &m_btnDelete);
    
    CreateLabel(L"Box Size (X,Y,Z):", &m_labelBox);
    CreateEdit(L"1.0", ID_EDIT_BOX_X, &m_editBoxX);
    CreateEdit(L"1.0", ID_EDIT_BOX_Y, &m_editBoxY);
    CreateEdit(L"1.0", ID_EDIT_BOX_Z, &m_editBoxZ);
    CreateButton(L"Create Box", ID_BTN_CREATE_BOX, &m_btnCreateBox);

    // --- Scene Mode ---
    CreateLabel(L"SCENE MODE", &m_labelSceneGroup, true);
    CreateDivider();
    CreateLabel(L"Select Scene:", &m_labelSceneMode);
    CreateButton(L"Default", ID_BTN_SCENE_DEFAULT, &m_btnSceneDefault);
    CreateButton(L"Solar System", ID_BTN_SCENE_SOLAR, &m_btnSceneSolarSystem);
    CreateButton(L"Crossroad", ID_BTN_SCENE_CROSSROAD, &m_btnSceneCrossroad);

    // --- Simulation Speed ---
    CreateLabel(L"SIMULATION SPEED", &m_labelSpeedGroup, true);
    CreateDivider();
    CreateLabel(L"Time Scale:", &m_labelSpeed);
    CreateButton(L"||", ID_BTN_SPEED_PAUSE, &m_btnSpeedPause);
    CreateButton(L"0.5x", ID_BTN_SPEED_05, &m_btnSpeed05);
    CreateButton(L"1x", ID_BTN_SPEED_1, &m_btnSpeed1);
    CreateButton(L"2x", ID_BTN_SPEED_2, &m_btnSpeed2);
    CreateButton(L"5x", ID_BTN_SPEED_5, &m_btnSpeed5);

    // --- Performance Mode ---
    CreateLabel(L"PERFORMANCE", &m_labelPerfGroup, true);
    CreateDivider();
    CreateLabel(L"Collision Detection:", &m_labelPerformance);
    CreateButton(L"High", ID_BTN_PERF_HIGH, &m_btnPerfHigh);
    CreateButton(L"Medium", ID_BTN_PERF_MEDIUM, &m_btnPerfMedium);
    CreateButton(L"Low", ID_BTN_PERF_LOW, &m_btnPerfLow);

    // --- Asteroid Controls ---
    CreateLabel(L"ASTEROID (Solar)", &m_labelAsteroidGroup, true);
    CreateDivider();
    CreateLabel(L"Velocity (X,Y,Z):", &m_labelAsteroidVelocity);
    CreateEdit(L"5.0", ID_EDIT_ASTEROID_VX, &m_editAsteroidVX);
    CreateEdit(L"0.0", ID_EDIT_ASTEROID_VY, &m_editAsteroidVY);
    CreateEdit(L"0.0", ID_EDIT_ASTEROID_VZ, &m_editAsteroidVZ);
    CreateLabel(L"Radius:", &m_labelAsteroidRadius);
    CreateEdit(L"0.3", ID_EDIT_ASTEROID_RADIUS, &m_editAsteroidRadius);
    CreateButton(L"Create Asteroid", ID_BTN_CREATE_ASTEROID, &m_btnCreateAsteroid);

    // --- Vehicle Controls ---
    CreateLabel(L"VEHICLE (Crossroad)", &m_labelVehicleGroup, true);
    CreateDivider();
    CreateLabel(L"Type:", &m_labelVehicleType);
    CreateCombo(ID_COMBO_VEHICLE_TYPE, &m_comboVehicleType, {L"Sedan", L"SUV", L"Truck", L"Bus", L"SportsCar"});
    
    CreateLabel(L"From:", &m_labelVehicleDirection);
    CreateCombo(ID_COMBO_VEHICLE_DIR, &m_comboVehicleDirection, {L"North", L"South", L"East", L"West"});
    
    CreateLabel(L"Action:", &m_labelVehicleIntention);
    CreateCombo(ID_COMBO_VEHICLE_INTENT, &m_comboVehicleIntention, {L"Straight", L"Turn Left", L"Turn Right"});
    
    CreateLabel(L"Speed:", &m_labelVehicleSpeed);
    CreateEdit(L"3.0", ID_EDIT_VEHICLE_SPEED, &m_editVehicleSpeed);
    
    CreateLabel(L"Scale:", &m_labelOBJScale);
    CreateEdit(L"1.0", ID_EDIT_OBJ_SCALE, &m_editOBJScale);
    
    CreateButton(L"Add Vehicle", ID_BTN_CREATE_VEHICLE, &m_btnCreateVehicle);
    CreateButton(L"Load OBJ...", ID_BTN_LOAD_OBJ, &m_btnLoadOBJ);

    // --- Properties ---
    CreateLabel(L"PROPERTIES", &m_labelPropsGroup, true);
    CreateDivider();
    CreateLabel(L"Selected:", &m_labelProperties);
    CreateLabel(L"None", &m_labelObjectName);
    CreateLabel(L"Position (X,Y,Z):", &m_labelPos);
    CreateEdit(L"0.00", 0, &m_editPosX, true);
    CreateEdit(L"0.00", 0, &m_editPosY, true);
    CreateEdit(L"0.00", 0, &m_editPosZ, true);
    CreateLabel(L"Rotation Y (deg):", &m_labelRotY);
    CreateEdit(L"0.0", 0, &m_editRotY, true);

    // --- Status Bar ---
    m_statusBar = CreateWindowExW(0, L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 0, 0, m_hwnd, nullptr, m_hInstance, nullptr);
    SendMessage(m_statusBar, WM_SETFONT, (WPARAM)m_hFontSmall, TRUE);

    // --- Info Panels (Moved to Left Panel to fix flickering) ---
    // System Status
    m_statusPanel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, m_panelContent, nullptr, m_hInstance, nullptr);
        
    HFONT hConsolas = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    SendMessage(m_statusPanel, WM_SETFONT, (WPARAM)hConsolas, TRUE);

    // Collision Stats
    m_overlayLabel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, m_panelContent, nullptr, m_hInstance, nullptr);
    SendMessage(m_overlayLabel, WM_SETFONT, (WPARAM)hConsolas, TRUE);
}

void Window::UpdateLayout()
{
    // 1. Position the Panel Container (Fixed Left Side)
    const int panelWidth = 300;
    const int statusBarHeight = 25;
    int containerHeight = m_height - statusBarHeight;
    if (containerHeight < 0) containerHeight = 0;
    
    SetWindowPos(m_panelContainer, nullptr, 0, 0, panelWidth, containerHeight, SWP_NOZORDER);

    // 2. Layout Controls inside Panel Content
    // Layout constants
    const int padding = 10;
    const int controlHeight = 28;
    const int labelHeight = 20;
    const int spacing = 12;
    const int groupSpacing = 20;
    const int dividerHeight = 1;
    
    int y = padding;
    int x = padding;
    int w = panelWidth - 2 * padding; // Width inside the content panel (minus scrollbar usually? No, Container has scrollbar)
    // If scrollbar is visible, client area of container is smaller. 
    // But we hardcode content width for now. 
    // Actually, let's get client rect of container to be precise.
    RECT rcContainer;
    GetClientRect(m_panelContainer, &rcContainer);
    if (rcContainer.right > 0) w = rcContainer.right - 2 * padding;

    size_t divIndex = 0;

    auto Place = [&](HWND hwnd, int height, int width = -1, int xOffset = 0) {
        int actualWidth = (width == -1) ? w : width;
        SetWindowPos(hwnd, nullptr, x + xOffset, y, actualWidth, height, SWP_NOZORDER);
        y += height + spacing;
    };
    
    auto PlaceDivider = [&]() {
        if (divIndex < m_dividers.size()) {
             SetWindowPos(m_dividers[divIndex], nullptr, x, y, w, dividerHeight, SWP_NOZORDER);
             divIndex++;
             y += dividerHeight + spacing;
        }
    };

    auto PlaceInline = [&](HWND hwnd, int height, int width, int xPos, bool newLine) {
        // Center vertically in the standard row height
        int rowH = controlHeight;
        int yOffset = (rowH - height) / 2;
        
        SetWindowPos(hwnd, nullptr, x + xPos, y + yOffset, width, height, SWP_NOZORDER);
        if (newLine) y += rowH + spacing;
    };

    // --- Basic Objects ---
    Place(m_labelBasicGroup, labelHeight);
    PlaceDivider();
    
    // Sphere row
    PlaceInline(m_labelSphereRadius, controlHeight, 100, 0, false);
    PlaceInline(m_editSphereRadius, 22, 60, 105, false);
    PlaceInline(m_btnCreateSphere, controlHeight, 85, 175, true);
    
    // Box row
    Place(m_labelBox, labelHeight);
    int boxEditW = (w - 2 * spacing) / 3;
    PlaceInline(m_editBoxX, 22, boxEditW, 0, false);
    PlaceInline(m_editBoxY, 22, boxEditW, boxEditW + spacing, false);
    PlaceInline(m_editBoxZ, 22, boxEditW, (boxEditW + spacing) * 2, true);
    Place(m_btnCreateBox, controlHeight);
    
    Place(m_btnDelete, controlHeight);
    y += groupSpacing;

    // --- Scene Mode ---
    Place(m_labelSceneGroup, labelHeight);
    PlaceDivider();
    Place(m_labelSceneMode, labelHeight);
    int btnW = (w - 2 * spacing) / 3;
    PlaceInline(m_btnSceneDefault, controlHeight, btnW, 0, false);
    PlaceInline(m_btnSceneSolarSystem, controlHeight, btnW, btnW + spacing, false);
    PlaceInline(m_btnSceneCrossroad, controlHeight, btnW, (btnW + spacing) * 2, true);
    y += groupSpacing;

    // --- Speed ---
    Place(m_labelSpeedGroup, labelHeight);
    PlaceDivider();
    Place(m_labelSpeed, labelHeight);
    int speedBtnW = (w - 4 * 2) / 5;
    PlaceInline(m_btnSpeedPause, controlHeight, speedBtnW, 0, false);
    PlaceInline(m_btnSpeed05, controlHeight, speedBtnW, speedBtnW + 2, false);
    PlaceInline(m_btnSpeed1, controlHeight, speedBtnW, (speedBtnW + 2) * 2, false);
    PlaceInline(m_btnSpeed2, controlHeight, speedBtnW, (speedBtnW + 2) * 3, false);
    PlaceInline(m_btnSpeed5, controlHeight, speedBtnW, (speedBtnW + 2) * 4, true);
    y += groupSpacing;

    // --- Performance ---
    Place(m_labelPerfGroup, labelHeight);
    PlaceDivider();
    Place(m_labelPerformance, labelHeight);
    PlaceInline(m_btnPerfHigh, controlHeight, btnW, 0, false);
    PlaceInline(m_btnPerfMedium, controlHeight, btnW, btnW + spacing, false);
    PlaceInline(m_btnPerfLow, controlHeight, btnW, (btnW + spacing) * 2, true);
    y += groupSpacing;

    // --- Asteroid ---
    Place(m_labelAsteroidGroup, labelHeight);
    PlaceDivider();
    Place(m_labelAsteroidVelocity, labelHeight);
    PlaceInline(m_editAsteroidVX, 22, boxEditW, 0, false);
    PlaceInline(m_editAsteroidVY, 22, boxEditW, boxEditW + spacing, false);
    PlaceInline(m_editAsteroidVZ, 22, boxEditW, (boxEditW + spacing) * 2, true);
    
    PlaceInline(m_labelAsteroidRadius, controlHeight, 60, 0, false);
    PlaceInline(m_editAsteroidRadius, 22, 60, 65, false);
    PlaceInline(m_btnCreateAsteroid, controlHeight, 125, 135, true);
    y += groupSpacing;

    // --- Vehicle ---
    Place(m_labelVehicleGroup, labelHeight);
    PlaceDivider();
    
    // Use a larger height for ComboBoxes so the dropdown list is visible
    const int comboHeight = 300; 

    // Vehicle Type
    PlaceInline(m_labelVehicleType, controlHeight, 50, 0, false);
    SetWindowPos(m_comboVehicleType, nullptr, x + 55, y, w - 55, comboHeight, SWP_NOZORDER);
    y += controlHeight + spacing;
    
    // Vehicle Direction
    PlaceInline(m_labelVehicleDirection, controlHeight, 50, 0, false);
    SetWindowPos(m_comboVehicleDirection, nullptr, x + 55, y, w - 55, comboHeight, SWP_NOZORDER);
    y += controlHeight + spacing;
    
    // Vehicle Intention
    PlaceInline(m_labelVehicleIntention, controlHeight, 50, 0, false);
    SetWindowPos(m_comboVehicleIntention, nullptr, x + 55, y, w - 55, comboHeight, SWP_NOZORDER);
    y += controlHeight + spacing;
    
    PlaceInline(m_labelVehicleSpeed, controlHeight, 50, 0, false);
    PlaceInline(m_editVehicleSpeed, 22, 60, 55, false);
    PlaceInline(m_labelOBJScale, controlHeight, 40, 125, false);
    PlaceInline(m_editOBJScale, 22, 50, 170, true);
    
    PlaceInline(m_btnCreateVehicle, controlHeight, 120, 0, false);
    PlaceInline(m_btnLoadOBJ, controlHeight, 120, 130, true);
    y += groupSpacing;

    // --- Properties ---
    Place(m_labelPropsGroup, labelHeight);
    PlaceDivider();
    PlaceInline(m_labelProperties, labelHeight, 70, 0, false);
    PlaceInline(m_labelObjectName, labelHeight, w - 75, 75, true);
    
    Place(m_labelPos, labelHeight);
    PlaceInline(m_editPosX, 22, boxEditW, 0, false);
    PlaceInline(m_editPosY, 22, boxEditW, boxEditW + spacing, false);
    PlaceInline(m_editPosZ, 22, boxEditW, (boxEditW + spacing) * 2, true);
    
    PlaceInline(m_labelRotY, controlHeight, 110, 0, false);
    PlaceInline(m_editRotY, 22, w - 115, 115, true);
    
    y += groupSpacing;

    // --- System Info (Moved to Panel) ---
    Place(m_statusPanel, 120); // Height for status text
    y += spacing;
    Place(m_overlayLabel, 60); // Height for collision stats

    // Update Total Height
    m_totalContentHeight = y + padding;

    // 3. Configure Scrollbar
    SCROLLINFO si = {};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    si.nMin = 0;
    si.nMax = m_totalContentHeight;
    si.nPage = containerHeight;
    si.nPos = m_scrollOffset;
    SetScrollInfo(m_panelContainer, SB_VERT, &si, TRUE);

    // 4. Position Panel Content
    // Ensure offset is valid
    if (m_scrollOffset > m_totalContentHeight - containerHeight) m_scrollOffset = m_totalContentHeight - containerHeight;
    if (m_scrollOffset < 0) m_scrollOffset = 0;
    
    SetWindowPos(m_panelContent, nullptr, 0, -m_scrollOffset, panelWidth, m_totalContentHeight, SWP_NOZORDER);

    // --- Status Bar ---
    SetWindowPos(m_statusBar, nullptr, 0, m_height - 25, m_width, 25, SWP_NOZORDER);
}

void Window::Show(int nCmdShow)
{
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Window* window = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hwnd = hwnd;
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) return window->HandleMessage(uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE:
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);
        UpdateLayout();
        if (OnResize) OnResize(m_width, m_height);
        return 0;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* pMinMaxInfo = (MINMAXINFO*)lParam;
        pMinMaxInfo->ptMinTrackSize.x = 1024;
        pMinMaxInfo->ptMinTrackSize.y = 768;
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hwndCtl = (HWND)lParam;
        SetTextColor(hdc, m_textColor);
        
        // Status Bar (Special Color)
        if (hwndCtl == m_statusBar) {
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(0, 122, 204)); // Blue status bar
            return (LRESULT)m_hbrAccent;
        }

        // Dividers
        for (HWND hDiv : m_dividers) {
            if (hwndCtl == hDiv) {
                SetBkMode(hdc, OPAQUE);
                SetBkColor(hdc, RGB(65, 65, 65));
                return (LRESULT)m_hbrButton;
            }
        }
        
        // Default Labels on Panel (Force Opaque to prevent scrolling glitches)
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(30, 30, 30)); // Match panel background
        return (LRESULT)m_hbrBackground;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, m_textColor);
        SetBkColor(hdc, RGB(60, 60, 60));
        return (LRESULT)m_hbrEdit;
    }
    
    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, m_textColor);
        SetBkColor(hdc, RGB(60, 60, 60));
        return (LRESULT)m_hbrEdit;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
        HDC hdc = lpDrawItem->hDC;
        RECT rc = lpDrawItem->rcItem;

        if (lpDrawItem->CtlType == ODT_BUTTON)
        {
            bool selected = lpDrawItem->itemState & ODS_SELECTED;
            bool hovered = (lpDrawItem->hwndItem == m_hoveredButton);

            // Select brush
            HBRUSH hbr = m_hbrButton;
            if (selected) hbr = m_hbrButtonActive;
            else if (hovered) hbr = m_hbrButtonHover;
            
            // Special case for primary action buttons
            int id = lpDrawItem->CtlID;
            if (id == ID_BTN_CREATE_SPHERE || id == ID_BTN_CREATE_BOX || 
                id == ID_BTN_CREATE_ASTEROID || id == ID_BTN_CREATE_VEHICLE)
            {
                if (!selected && !hovered) hbr = m_hbrAccent;
            }

            // 1. Clear background (fix white corners)
            FillRect(hdc, &rc, m_hbrPanel);

            // 2. Draw Rounded Rect (Borderless)
            SelectObject(hdc, GetStockObject(NULL_PEN));
            SelectObject(hdc, hbr);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);

            // Draw text
            wchar_t text[256];
            GetWindowTextW(lpDrawItem->hwndItem, text, 256);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, m_textColor);
            SelectObject(hdc, m_hFont);
            
            // Measure text for precise centering
            RECT rcTextCalc = rc;
            DrawTextW(hdc, text, -1, &rcTextCalc, DT_CENTER | DT_CALCRECT | DT_SINGLELINE);
            
            int textH = rcTextCalc.bottom - rcTextCalc.top;
            int btnH = rc.bottom - rc.top;
            int y = rc.top + (btnH - textH) / 2;
            
            RECT rcText = rc;
            rcText.top = y;
            rcText.bottom = y + textH;
            
            DrawTextW(hdc, text, -1, &rcText, DT_CENTER | DT_SINGLELINE | DT_TOP);
            
            return TRUE;
        }
        else if (lpDrawItem->CtlType == ODT_COMBOBOX)
        {
            // Background
            bool selected = lpDrawItem->itemState & ODS_SELECTED;
            
            if (selected) {
                FillRect(hdc, &rc, m_hbrAccent); // Blue highlight
                SetTextColor(hdc, RGB(255, 255, 255));
            } else {
                // Use edit brush (dark grey) for background
                FillRect(hdc, &rc, m_hbrEdit); 
                SetTextColor(hdc, m_textColor);
            }

            // Text
            if (lpDrawItem->itemID != -1) {
                wchar_t text[256];
                // Get text from combo
                SendMessageW(lpDrawItem->hwndItem, CB_GETLBTEXT, lpDrawItem->itemID, (LPARAM)text);
                
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, m_hFont);
                
                RECT rcText = rc;
                rcText.left += 5; // Padding
                DrawTextW(hdc, text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            return TRUE;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        // Handle button hover effects
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        HWND hWndPoint = ChildWindowFromPoint(m_hwnd, pt);
        
        if (hWndPoint != m_hoveredButton)
        {
            if (m_hoveredButton) InvalidateRect(m_hoveredButton, nullptr, TRUE);
            m_hoveredButton = hWndPoint;
            if (m_hoveredButton) InvalidateRect(m_hoveredButton, nullptr, TRUE);
            
            // Track mouse leave
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = m_hwnd;
            TrackMouseEvent(&tme);
        }
        
        // Pass to camera control
        m_mouseX = pt.x;
        m_mouseY = pt.y;
        m_mouseDX += m_mouseX - m_lastMouseX;
        m_mouseDY += m_mouseY - m_lastMouseY;
        m_lastMouseX = m_mouseX;
        m_lastMouseY = m_mouseY;
        return 0;
    }
    
    case WM_MOUSELEAVE:
        if (m_hoveredButton)
        {
            InvalidateRect(m_hoveredButton, nullptr, TRUE);
            m_hoveredButton = nullptr;
        }
        return 0;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_BTN_CREATE_SPHERE:
            if (OnCreateSphere) OnCreateSphere(GetFloatFromEdit(m_editSphereRadius, 1.0f));
            break;
        case ID_BTN_CREATE_BOX:
            if (OnCreateBox) OnCreateBox(
                GetFloatFromEdit(m_editBoxX, 1.0f),
                GetFloatFromEdit(m_editBoxY, 1.0f),
                GetFloatFromEdit(m_editBoxZ, 1.0f));
            break;
        case ID_BTN_DELETE:
            if (OnDeleteObject) OnDeleteObject();
            break;
        case ID_BTN_SCENE_DEFAULT:
            if (OnSceneModeChanged) OnSceneModeChanged(0);
            break;
        case ID_BTN_SCENE_SOLAR:
            if (OnSceneModeChanged) OnSceneModeChanged(1);
            break;
        case ID_BTN_SCENE_CROSSROAD:
            if (OnSceneModeChanged) OnSceneModeChanged(2);
            break;
        case ID_BTN_SPEED_PAUSE:
            if (OnSimulationSpeedChanged) OnSimulationSpeedChanged(0.0f);
            break;
        case ID_BTN_SPEED_05:
            if (OnSimulationSpeedChanged) OnSimulationSpeedChanged(0.5f);
            break;
        case ID_BTN_SPEED_1:
            if (OnSimulationSpeedChanged) OnSimulationSpeedChanged(1.0f);
            break;
        case ID_BTN_SPEED_2:
            if (OnSimulationSpeedChanged) OnSimulationSpeedChanged(2.0f);
            break;
        case ID_BTN_SPEED_5:
            if (OnSimulationSpeedChanged) OnSimulationSpeedChanged(5.0f);
            break;
        case ID_BTN_PERF_HIGH:
            if (OnPerformanceModeChanged) OnPerformanceModeChanged(0);
            break;
        case ID_BTN_PERF_MEDIUM:
            if (OnPerformanceModeChanged) OnPerformanceModeChanged(1);
            break;
        case ID_BTN_PERF_LOW:
            if (OnPerformanceModeChanged) OnPerformanceModeChanged(2);
            break;
        case ID_BTN_CREATE_ASTEROID:
            if (OnCreateAsteroid) OnCreateAsteroid(
                GetFloatFromEdit(m_editAsteroidVX, 5.0f),
                GetFloatFromEdit(m_editAsteroidVY, 0.0f),
                GetFloatFromEdit(m_editAsteroidVZ, 0.0f),
                GetFloatFromEdit(m_editAsteroidRadius, 0.3f));
            break;
        case ID_BTN_CREATE_VEHICLE:
            if (OnCreateVehicle) OnCreateVehicle(
                (int)SendMessageW(m_comboVehicleType, CB_GETCURSEL, 0, 0),
                (int)SendMessageW(m_comboVehicleDirection, CB_GETCURSEL, 0, 0),
                (int)SendMessageW(m_comboVehicleIntention, CB_GETCURSEL, 0, 0),
                GetFloatFromEdit(m_editVehicleSpeed, 3.0f));
            break;
        case ID_BTN_LOAD_OBJ:
        {
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
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL);
                std::string objPath(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, filename, -1, &objPath[0], size_needed, NULL, NULL);

                if (OnLoadVehicleFromOBJ) OnLoadVehicleFromOBJ(
                    objPath,
                    (int)SendMessageW(m_comboVehicleDirection, CB_GETCURSEL, 0, 0),
                    (int)SendMessageW(m_comboVehicleIntention, CB_GETCURSEL, 0, 0),
                    GetFloatFromEdit(m_editVehicleSpeed, 3.0f),
                    GetFloatFromEdit(m_editOBJScale, 1.0f));
            }
            break;
        }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_F1) { ShowHelpDialog(); return 0; }
        if (wParam < 256) m_keys[wParam] = true;
        return 0;

    case WM_KEYUP:
        if (wParam < 256) m_keys[wParam] = false;
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

    case WM_MOUSEWHEEL:
        m_mouseWheel += GET_WHEEL_DELTA_WPARAM(wParam);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

float Window::GetFloatFromEdit(HWND hEdit, float defaultValue)
{
    wchar_t buffer[32];
    GetWindowTextW(hEdit, buffer, 32);
    float val = static_cast<float>(_wtof(buffer));
    return (val == 0.0f && buffer[0] != '0') ? defaultValue : val;
}

void Window::SetStatusText(const std::wstring& text)
{
    if (m_statusBar) SetWindowTextW(m_statusBar, text.c_str());
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
                                  float posX, float posY, float posZ, float rotY)
{
    if (!m_labelObjectName) return;

    if (selectedIndex == static_cast<size_t>(-1))
    {
        SetWindowTextW(m_labelObjectName, L"None");
        SetWindowTextW(m_editPosX, L"-");
        SetWindowTextW(m_editPosY, L"-");
        SetWindowTextW(m_editPosZ, L"-");
        SetWindowTextW(m_editRotY, L"-");
    }
    else
    {
        wchar_t buffer[256];
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, objectName.c_str(), -1, NULL, 0);
        std::wstring wObjectName(size_needed - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, objectName.c_str(), -1, &wObjectName[0], size_needed);
        SetWindowTextW(m_labelObjectName, wObjectName.c_str());

        swprintf_s(buffer, L"%.2f", posX); SetWindowTextW(m_editPosX, buffer);
        swprintf_s(buffer, L"%.2f", posY); SetWindowTextW(m_editPosY, buffer);
        swprintf_s(buffer, L"%.2f", posZ); SetWindowTextW(m_editPosZ, buffer);
        
        float rotDegrees = rotY * 180.0f / 3.14159265f;
        swprintf_s(buffer, L"%.1f", rotDegrees); SetWindowTextW(m_editRotY, buffer);
    }
}

void Window::SetOverlayText(const std::wstring& text)
{
    if (!m_overlayLabel) return;
    
    int len = GetWindowTextLengthW(m_overlayLabel);
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(m_overlayLabel, &buf[0], len + 1);
    if (text != &buf[0]) {
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
        L"------------------\n"                                                                                
        L"FPS: %.1f\n"                                                                                   
        L"Frame Time: %.2f ms\n"                                                                         
        L"Objects: %zu\n"                                                                                
        L"Scene: %s\n"                                                                                   
        L"Selected: %s",                                                                                 
        fps, frameTime, objectCount, sceneMode.c_str(), selectedObject.c_str());                         
                                                                                                         
    int len = GetWindowTextLengthW(m_statusPanel);
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(m_statusPanel, &buf[0], len + 1);
    if (std::wstring(statusText) != &buf[0]) {
        SetWindowTextW(m_statusPanel, statusText);
    }
}
LRESULT CALLBACK Window::PanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }

    if (!window) return DefWindowProc(hwnd, uMsg, wParam, lParam);

    // Forward specific messages to the main window
    if (uMsg == WM_COMMAND || uMsg == WM_CTLCOLORSTATIC || uMsg == WM_CTLCOLOREDIT || 
        uMsg == WM_CTLCOLORLISTBOX || uMsg == WM_DRAWITEM || uMsg == WM_NOTIFY)
    {
        return SendMessage(window->m_hwnd, uMsg, wParam, lParam);
    }

    // Scroll Logic (Only for Panel Container)
    if (hwnd == window->m_panelContainer)
    {
        // 1. Handle Paint (Custom Scrollbar + Background)
        if (uMsg == WM_PAINT)
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Draw Background
            FillRect(hdc, &rc, window->m_hbrBackground);
            
            // Draw Scrollbar if needed
            if (window->m_totalContentHeight > rc.bottom)
            {
                int trackW = 12;
                RECT rcTrack = { rc.right - trackW, 0, rc.right, rc.bottom };
                
                // Draw Track
                static HBRUSH hbrTrack = CreateSolidBrush(RGB(45, 45, 48));
                FillRect(hdc, &rcTrack, hbrTrack);
                
                // Calculate Thumb Geometry
                int viewH = rc.bottom;
                int contentH = window->m_totalContentHeight;
                // Minimum thumb height 30px
                int thumbH = max(30, (int)((float)viewH / contentH * viewH));
                
                int scrollableH = contentH - viewH;
                int trackScrollableH = viewH - thumbH;
                
                // Current thumb Y
                int thumbY = 0;
                if (scrollableH > 0)
                    thumbY = (int)((float)window->m_scrollOffset / scrollableH * trackScrollableH);
                
                RECT rcThumb = { rc.right - trackW + 3, thumbY, rc.right - 3, thumbY + thumbH };
                window->m_rcScrollThumb = rcThumb; // Save for hit test
                
                // Draw Thumb (Rounded)
                HBRUSH hbrThumb = window->m_isDraggingScroll ? window->m_hbrButtonActive : window->m_hbrButton;
                SelectObject(hdc, GetStockObject(NULL_PEN));
                SelectObject(hdc, hbrThumb);
                RoundRect(hdc, rcThumb.left, rcThumb.top, rcThumb.right, rcThumb.bottom, 4, 4);
            }
            else
            {
                // No scrollbar needed
                window->m_rcScrollThumb = { 0,0,0,0 };
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        // 2. Handle EraseBkgnd (Prevent flickering, handled in Paint)
        if (uMsg == WM_ERASEBKGND) return 1;

        // 3. Mouse Interaction
        if (uMsg == WM_LBUTTONDOWN)
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&window->m_rcScrollThumb, pt)) {
                window->m_isDraggingScroll = true;
                window->m_dragStartY = pt.y;
                window->m_initialScrollY = window->m_scrollOffset; // Store scroll offset at drag start
                // We store the thumb Y relative to mouse, but simpler to just track delta
                // Actually simpler: store the *pixel* offset of mouse on screen vs drag start
                SetCapture(hwnd);
                return 0;
            }
        }
        
        if (uMsg == WM_MOUSEMOVE && window->m_isDraggingScroll)
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int deltaY = pt.y - window->m_dragStartY;
            
            RECT rc; GetClientRect(hwnd, &rc);
            int viewH = rc.bottom;
            int contentH = window->m_totalContentHeight;
            int thumbH = max(30, (int)((float)viewH / contentH * viewH));
            int trackScrollableH = viewH - thumbH;
            int scrollableH = contentH - viewH;
            
            if (trackScrollableH > 0) {
                // Map pixel delta to scroll delta
                float scale = (float)scrollableH / trackScrollableH;
                int scrollDelta = (int)(deltaY * scale);
                
                int newScroll = window->m_initialScrollY + scrollDelta;
                if (newScroll < 0) newScroll = 0;
                if (newScroll > scrollableH) newScroll = scrollableH;
                
                if (window->m_scrollOffset != newScroll) {
                    window->m_scrollOffset = newScroll;
                    SetWindowPos(window->m_panelContent, nullptr, 0, -newScroll, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    InvalidateRect(hwnd, nullptr, FALSE); // Redraw scrollbar
                    InvalidateRect(window->m_panelContent, nullptr, TRUE); // Redraw content
                    UpdateWindow(window->m_panelContent);
                }
            }
            return 0;
        }
        
        if (uMsg == WM_LBUTTONUP && window->m_isDraggingScroll)
        {
            window->m_isDraggingScroll = false;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // 4. Mouse Wheel (Manual Scroll)
        if (uMsg == WM_MOUSEWHEEL)
        {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            int scrollAmount = -zDelta; // Negative delta = scroll down (pos increase)
            
            RECT rc; GetClientRect(hwnd, &rc);
            int maxScroll = window->m_totalContentHeight - rc.bottom;
            if (maxScroll < 0) maxScroll = 0;
            
            int newScroll = window->m_scrollOffset + scrollAmount;
            if (newScroll < 0) newScroll = 0;
            if (newScroll > maxScroll) newScroll = maxScroll;
            
            if (window->m_scrollOffset != newScroll) {
                window->m_scrollOffset = newScroll;
                SetWindowPos(window->m_panelContent, nullptr, 0, -newScroll, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                InvalidateRect(hwnd, nullptr, FALSE);
                InvalidateRect(window->m_panelContent, nullptr, TRUE);
                UpdateWindow(window->m_panelContent);
            }
            return 0;
        }
    }
    
    // Forward mouse wheel from Content to Container
    if (hwnd == window->m_panelContent && uMsg == WM_MOUSEWHEEL)
    {
        return SendMessage(window->m_panelContainer, uMsg, wParam, lParam);
    }

    // Paint background for Content Panel (if it needs painting, usually covered by controls or same bg)
    if (uMsg == WM_ERASEBKGND && hwnd == window->m_panelContent)
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, window->m_hbrBackground); 
        return 1;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
