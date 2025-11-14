#pragma once
#include <windows.h>
#include <string>
#include <functional>

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
    std::function<void(float)> OnCreateSphere;
    std::function<void(float, float, float)> OnCreateBox;
    std::function<void()> OnDeleteObject;
    std::function<void(int)> OnSceneModeChanged;  // 0=Default, 1=SolarSystem, 2=Crossroad
    std::function<void(float)> OnSimulationSpeedChanged;
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

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

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

    // UI controls
    HWND m_btnCreateSphere;
    HWND m_btnCreateBox;
    HWND m_btnDelete;
    HWND m_editSphereRadius;
    HWND m_editBoxX, m_editBoxY, m_editBoxZ;
    HWND m_labelSphereRadius;
    HWND m_labelBox;

    // Scene mode controls
    HWND m_labelSceneMode;
    HWND m_btnSceneDefault;
    HWND m_btnSceneSolarSystem;
    HWND m_btnSceneCrossroad;

    // Speed controls
    HWND m_labelSpeed;
    HWND m_btnSpeedPause;
    HWND m_btnSpeed05;
    HWND m_btnSpeed1;
    HWND m_btnSpeed2;
    HWND m_btnSpeed5;

    // Asteroid controls
    HWND m_labelAsteroid;
    HWND m_editAsteroidVX, m_editAsteroidVY, m_editAsteroidVZ, m_editAsteroidRadius;
    HWND m_btnCreateAsteroid;
    HWND m_labelAsteroidVelocity;
    HWND m_labelAsteroidRadius;

    // Vehicle controls (for crossroad scene)
    HWND m_labelVehicle;
    HWND m_labelVehicleType;
    HWND m_comboVehicleType;
    HWND m_labelVehicleDirection;
    HWND m_comboVehicleDirection;
    HWND m_labelVehicleIntention;
    HWND m_comboVehicleIntention;
    HWND m_labelVehicleSpeed;
    HWND m_editVehicleSpeed;
    HWND m_btnCreateVehicle;
    HWND m_btnLoadOBJ;
    HWND m_labelOBJScale;
    HWND m_editOBJScale;

    // Status bar
    HWND m_statusBar;

    // Properties panel
    HWND m_labelProperties;
    HWND m_labelObjectName;
    HWND m_labelPosX, m_labelPosY, m_labelPosZ;
    HWND m_editPosX, m_editPosY, m_editPosZ;
    HWND m_labelRotY;
    HWND m_editRotY;

    void CreateUIControls();
};
