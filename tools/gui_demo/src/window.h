#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <vector>

class Window
{
public:
    Window(HINSTANCE hInstance, const std::wstring& title, int width, int height);
    ~Window();

    bool Initialize();
    void Show(int nCmdShow);

    HWND GetHWND() const { return m_hwnd; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // UI events
    std::function<void(int, int)> OnResize;
    std::function<void(float)> OnCreateSphere;
    std::function<void(float, float, float)> OnCreateBox;
    std::function<void()> OnDeleteObject;
    std::function<void(int)> OnSceneModeChanged;  // 0=Default, 1=SolarSystem, 2=Crossroad
    std::function<void(float)> OnSimulationSpeedChanged;
    std::function<void(int)> OnPerformanceModeChanged;  // 0=High, 1=Medium, 2=Low
    std::function<void(float, float, float, float)> OnCreateAsteroid;  // vx, vy, vz, radius
    std::function<void(int, int, int, float)> OnCreateVehicle;  // vehicleType, direction, intention, speed
    std::function<void(std::string, int, int, float, float)> OnLoadVehicleFromOBJ;  // objPath, direction, intention, speed, scale

    // Input state
    bool IsKeyDown(int vkCode) const { return m_keys[vkCode]; }
    bool IsMouseButtonDown(int button) const { return m_mouseButtons[button]; }
    void GetMousePosition(int& x, int& y) const { x = m_mouseX; y = m_mouseY; }
    void GetMouseDelta(int& dx, int& dy) const { dx = m_mouseDX; dy = m_mouseDY; }
    int GetMouseWheel() const { return m_mouseWheel; }
    void ResetMouseInput() { m_mouseDX = 0; m_mouseDY = 0; m_mouseWheel = 0; }

    // Status bar
    void SetStatusText(const std::wstring& text);

    // Help dialog
    void ShowHelpDialog();

    // Properties panel
    void UpdatePropertiesPanel(size_t selectedIndex, const std::string& objectName,
                              float posX, float posY, float posZ,
                              float rotY);

    // Overlay diagnostics for collision stats
    void SetOverlayText(const std::wstring& text);

    // Enhanced status panel
    void UpdateStatusPanel(float fps, float frameTime, size_t objectCount,
                           const std::wstring& sceneMode, const std::wstring& selectedObject);

    // Access to raw overlay label (for advanced overlays)
    HWND GetOverlayHandle() const { return m_overlayLabel; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void CreateUIControls();
    void UpdateLayout();
    float GetFloatFromEdit(HWND hEdit, float defaultValue);

    HINSTANCE m_hInstance;
    HWND m_hwnd;
    std::wstring m_title;
    int m_width;
    int m_height;

    // Input state
    bool m_keys[256];
    bool m_mouseButtons[3]; // Left, Right, Middle
    int m_mouseX, m_mouseY;
    int m_mouseDX, m_mouseDY;
    int m_lastMouseX, m_lastMouseY;
    int m_mouseWheel;

    // Theme resources
    HBRUSH m_hbrBackground;
    HBRUSH m_hbrPanel;
    HBRUSH m_hbrEdit;
    HBRUSH m_hbrButton;
    HBRUSH m_hbrButtonHover;
    HBRUSH m_hbrButtonActive;
    HBRUSH m_hbrAccent;
    COLORREF m_textColor;
    COLORREF m_accentColor;
    HFONT m_hFont;
    HFONT m_hFontBold;
    HFONT m_hFontSmall;

    // UI controls
    // Basic Objects
    HWND m_labelBasicGroup;
    HWND m_labelSphereRadius;
    HWND m_editSphereRadius;
    HWND m_btnCreateSphere;
    HWND m_btnDelete;
    HWND m_labelBox;
    HWND m_editBoxX, m_editBoxY, m_editBoxZ;
    HWND m_btnCreateBox;

    // Scene Mode
    HWND m_labelSceneGroup;
    HWND m_labelSceneMode;
    HWND m_btnSceneDefault;
    HWND m_btnSceneSolarSystem;
    HWND m_btnSceneCrossroad;

    // Simulation Speed
    HWND m_labelSpeedGroup;
    HWND m_labelSpeed;
    HWND m_btnSpeedPause;
    HWND m_btnSpeed05;
    HWND m_btnSpeed1;
    HWND m_btnSpeed2;
    HWND m_btnSpeed5;

    // Performance Mode
    HWND m_labelPerfGroup;
    HWND m_labelPerformance;
    HWND m_btnPerfHigh;
    HWND m_btnPerfMedium;
    HWND m_btnPerfLow;

    // Asteroid controls
    HWND m_labelAsteroidGroup;
    HWND m_labelAsteroidVelocity;
    HWND m_editAsteroidVX, m_editAsteroidVY, m_editAsteroidVZ;
    HWND m_labelAsteroidRadius;
    HWND m_editAsteroidRadius;
    HWND m_btnCreateAsteroid;

    // Vehicle controls
    HWND m_labelVehicleGroup;
    HWND m_labelVehicleType;
    HWND m_comboVehicleType;
    HWND m_labelVehicleDirection;
    HWND m_comboVehicleDirection;
    HWND m_labelVehicleIntention;
    HWND m_comboVehicleIntention;
    HWND m_labelVehicleSpeed;
    HWND m_editVehicleSpeed;
    HWND m_labelOBJScale;
    HWND m_editOBJScale;
    HWND m_btnCreateVehicle;
    HWND m_btnLoadOBJ;

    // Properties panel
    HWND m_labelPropsGroup;
    HWND m_labelProperties;
    HWND m_labelObjectName;
    HWND m_labelPos;
    HWND m_editPosX, m_editPosY, m_editPosZ;
    HWND m_labelRotY;
    HWND m_editRotY;

    // Status bar
    HWND m_statusBar;

    // Overlay diagnostics (semi-transparent label over 3D view)
    HWND m_overlayLabel;

    // Enhanced status panel (semi-transparent, detailed info)
    HWND m_statusPanel;
    HWND m_statusPanelBackground;
    
    // Scrollable Left Panel
    HWND m_panelContainer; // The visible clipping area
    HWND m_panelContent;   // The tall window holding all controls
    int m_scrollOffset;
    int m_totalContentHeight;
    static LRESULT CALLBACK PanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Track hovered button for redraw
    HWND m_hoveredButton;

    // Dividers
    std::vector<HWND> m_dividers;
};
