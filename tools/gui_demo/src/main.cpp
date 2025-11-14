// FCL Collision Demo - Windows GUI Application
// 3D visualization and collision detection demo with interactive wizard

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <vector>
#include <memory>
#include <string>
#include "window.h"
#include "renderer.h"
#include "scene.h"
#include "fcl_driver.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

// Application entry point
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        MessageBoxA(nullptr, "Failed to initialize COM", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    try
    {
        // Create window
        auto window = std::make_unique<Window>(hInstance, L"FCL Collision Demo", 1280, 720);
        if (!window->Initialize())
        {
            MessageBoxA(nullptr, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
            return -1;
        }

        // Create renderer
        auto renderer = std::make_unique<Renderer>();
        if (!renderer->Initialize(window->GetHWND(), window->GetWidth(), window->GetHeight()))
        {
            MessageBoxA(nullptr, "Failed to initialize renderer", "Error", MB_OK | MB_ICONERROR);
            return -1;
        }

        // Initialize FCL driver connection
        auto driver = std::make_unique<FclDriver>();
        if (!driver->Connect(L"\\\\.\\FclMusa"))
        {
            MessageBoxA(nullptr,
                "Failed to connect to FCL driver.\n\n"
                "Please make sure the FclMusa driver is loaded:\n"
                "  sc start FclMusa\n\n"
                "Or run as Administrator.",
                "Driver Connection Error", MB_OK | MB_ICONWARNING);
            // Continue anyway for UI testing
        }

        // Create scene
        auto scene = std::make_unique<Scene>(renderer.get(), driver.get());
        scene->Initialize();

        // Show window
        window->Show(nCmdShow);

        // Main message loop
        MSG msg = {};
        LARGE_INTEGER frequency, lastTime, currentTime;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);

        while (msg.message != WM_QUIT)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                // Calculate delta time
                QueryPerformanceCounter(&currentTime);
                float deltaTime = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) /
                                  static_cast<float>(frequency.QuadPart);
                lastTime = currentTime;

                // Handle input
                scene->HandleInput(window.get(), deltaTime);

                // Update and render
                scene->Update(deltaTime);

                renderer->BeginFrame();
                scene->Render();
                renderer->EndFrame();
            }
        }

        // Cleanup
        scene.reset();
        renderer.reset();
        driver.reset();
        window.reset();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "Exception", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return -1;
    }

    CoUninitialize();
    return 0;
}
