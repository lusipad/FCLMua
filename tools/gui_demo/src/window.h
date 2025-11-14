#pragma once
#include <windows.h>
#include <string>

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

    // Input state
    bool IsKeyDown(int vkCode) const { return m_keys[vkCode]; }
    bool IsMouseButtonDown(int button) const { return m_mouseButtons[button]; }
    void GetMousePosition(int& x, int& y) const { x = m_mouseX; y = m_mouseY; }
    void GetMouseDelta(int& dx, int& dy) const { dx = m_mouseDX; dy = m_mouseDY; }
    int GetMouseWheel() const { return m_mouseWheel; }

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
};
