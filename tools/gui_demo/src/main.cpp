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
        // Get module handle (safer than relying on hInstance parameter)
        HINSTANCE hInst = GetModuleHandle(NULL);
        if (!hInst)
        {
            MessageBoxA(nullptr, "Failed to get module handle", "Error", MB_OK | MB_ICONERROR);
            return -1;
        }

        // Create window (use a slightly larger default size so that
        // left-side controls are fully visible without manual resize)
        auto window = std::make_unique<Window>(hInst, L"FCL Collision Demo", 1400, 900);
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

        // Prime kernel diagnostics once to avoid showing empty data for users
        FCL_DIAGNOSTICS_RESPONSE initialDiag = {};
        driver->QueryDiagnostics(initialDiag);

        // Connect UI events to scene
        window->OnCreateSphere = [&scene, &window](float radius) {
            XMFLOAT3 pos(0, 2, 0);
            std::string name = "Sphere " + std::to_string(scene->GetObjectCount() + 1);
            scene->AddSphere(name, pos, radius);
            scene->SelectObject(scene->GetObjectCount() - 1);

            wchar_t msg[128];
            swprintf_s(msg, L"Created sphere with radius %.2f", radius);
            window->SetStatusText(msg);
        };

        window->OnCreateBox = [&scene, &window](float x, float y, float z) {
            XMFLOAT3 pos(0, 2, 0);
            XMFLOAT3 extents(x, y, z);
            std::string name = "Box " + std::to_string(scene->GetObjectCount() + 1);
            scene->AddBox(name, pos, extents);
            scene->SelectObject(scene->GetObjectCount() - 1);

            wchar_t msg[128];
            swprintf_s(msg, L"Created box with size (%.2f, %.2f, %.2f)", x, y, z);
            window->SetStatusText(msg);
        };

        window->OnDeleteObject = [&scene, &window]() {
            size_t selected = scene->GetSelectedObjectIndex();
            if (selected != static_cast<size_t>(-1))
            {
                scene->DeleteObject(selected);
                window->SetStatusText(L"Object deleted");
            }
            else
            {
                window->SetStatusText(L"No object selected to delete");
            }
        };

        window->OnSceneModeChanged = [&scene, &window](int mode) {
            if (mode == 0)
            {
                scene->SetSceneMode(SceneMode::Default);
                window->SetStatusText(L"Switched to Default scene");
            }
            else if (mode == 1)
            {
                scene->SetSceneMode(SceneMode::SolarSystem);
                window->SetStatusText(L"Switched to Solar System scene");
            }
            else if (mode == 2)
            {
                scene->SetSceneMode(SceneMode::CrossroadSimulation);
                window->SetStatusText(L"Switched to Crossroad Simulation scene");
            }
        };

        window->OnSimulationSpeedChanged = [&scene, &window](float speed) {
            scene->SetSimulationSpeed(speed);
            wchar_t msg[128];
            if (speed == 0.0f)
                swprintf_s(msg, L"Simulation paused");
            else
                swprintf_s(msg, L"Simulation speed: %.1fx", speed);
            window->SetStatusText(msg);
        };

        window->OnPerformanceModeChanged = [&scene, &window](int mode) {
            scene->SetPerformanceMode(static_cast<PerformanceMode>(mode));
            wchar_t msg[128];
            switch (mode)
            {
            case 0:
                swprintf_s(msg, L"Performance mode: High (every frame collision detection)");
                break;
            case 1:
                swprintf_s(msg, L"Performance mode: Medium (every 2nd frame)");
                break;
            case 2:
                swprintf_s(msg, L"Performance mode: Low (every 3rd frame, optimized for VMs)");
                break;
            default:
                swprintf_s(msg, L"Performance mode changed");
                break;
            }
            window->SetStatusText(msg);
        };

        window->OnCreateAsteroid = [&scene, &window](float vx, float vy, float vz, float radius) {
            // Create asteroid at camera target with specified velocity
            XMFLOAT3 pos = scene->GetCamera().GetTarget();
            pos.y += 2.0f; // Slightly above the target
            XMFLOAT3 velocity(vx, vy, vz);
            std::string name = "Asteroid " + std::to_string(scene->GetObjectCount() + 1);
            scene->AddAsteroid(name, pos, velocity, radius);
            scene->SelectObject(scene->GetObjectCount() - 1);

            wchar_t msg[128];
            swprintf_s(msg, L"Created asteroid with velocity (%.1f, %.1f, %.1f)", vx, vy, vz);
            window->SetStatusText(msg);
        };

        window->OnCreateVehicle = [&scene, &window](int vehicleType, int direction, int intention, float speed) {
            // Create vehicle with specified parameters
            std::string name = "Vehicle " + std::to_string(scene->GetObjectCount() + 1);
            scene->AddVehicle(name,
                static_cast<VehicleType>(vehicleType),
                static_cast<VehicleDirection>(direction),
                static_cast<MovementIntention>(intention),
                speed);
            scene->SelectObject(scene->GetObjectCount() - 1);
            window->SetStatusText(L"Created vehicle in crossroad scene");
        };

        window->OnLoadVehicleFromOBJ = [&scene, &window](std::string objPath, int direction, int intention, float speed, float scale) {
            // Load vehicle from OBJ file
            std::string name = "Custom " + std::to_string(scene->GetObjectCount() + 1);
            size_t before = scene->GetObjectCount();
            scene->AddVehicleFromOBJ(name, objPath,
                static_cast<VehicleDirection>(direction),
                static_cast<MovementIntention>(intention),
                speed, scale);

            // Check if loading was successful
            if (scene->GetObjectCount() > before)
            {
                scene->SelectObject(scene->GetObjectCount() - 1);
                window->SetStatusText(L"Successfully loaded custom vehicle from OBJ file");
            }
            else
            {
                window->SetStatusText(L"Failed to load OBJ file");
            }
        };

        // Show window
        window->Show(nCmdShow);

        // Main message loop with FPS tracking
        MSG msg = {};
        LARGE_INTEGER frequency, lastTime, currentTime;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);

        // FPS tracking variables
        int frameCount = 0;
        float fpsTimer = 0.0f;
        float currentFPS = 0.0f;
        float avgFrameTime = 0.0f;

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

                // Update FPS counter
                frameCount++;
                fpsTimer += deltaTime;

                // Update FPS display every 0.5 seconds
                if (fpsTimer >= 0.5f)
                {
                    currentFPS = frameCount / fpsTimer;
                    avgFrameTime = (fpsTimer / frameCount) * 1000.0f; // Convert to ms

                    // Update window title with performance stats
                    wchar_t titleBuffer[256];
                    swprintf_s(titleBuffer, L"FCL Collision Demo - FPS: %.1f | Frame: %.2f ms | Objects: %zu",
                              currentFPS, avgFrameTime, scene->GetObjectCount());
                    SetWindowTextW(window->GetHWND(), titleBuffer);

                    frameCount = 0;
                    fpsTimer = 0.0f;
                }

                // Handle input
                scene->HandleInput(window.get(), deltaTime);

                // Update scene (physics, vehicles, collisions)
                scene->Update(deltaTime);

                // Update properties panel with selected object info
                size_t selectedIdx = scene->GetSelectedObjectIndex();
                std::wstring selectedObjName = L"None";
                if (selectedIdx != static_cast<size_t>(-1))
                {
                    auto* obj = scene->GetObject(selectedIdx);
                    if (obj)
                    {
                        window->UpdatePropertiesPanel(selectedIdx, obj->name,
                                                     obj->position.x, obj->position.y, obj->position.z,
                                                     obj->rotation.y);
                        // Convert object name to wstring for status panel
                        int size_needed = MultiByteToWideChar(CP_UTF8, 0, obj->name.c_str(), -1, NULL, 0);
                        selectedObjName.resize(size_needed - 1);
                        MultiByteToWideChar(CP_UTF8, 0, obj->name.c_str(), -1, &selectedObjName[0], size_needed);
                    }
                }
                else
                {
                    window->UpdatePropertiesPanel(static_cast<size_t>(-1), "", 0, 0, 0, 0);
                }

                // Update enhanced status panel
                std::wstring sceneModeName;
                switch (scene->GetSceneMode())
                {
                case SceneMode::Default:
                    sceneModeName = L"Default";
                    break;
                case SceneMode::SolarSystem:
                    sceneModeName = L"Solar System";
                    break;
                case SceneMode::CrossroadSimulation:
                    sceneModeName = L"Crossroad";
                    break;
                default:
                    sceneModeName = L"Unknown";
                    break;
                }

                window->UpdateStatusPanel(currentFPS, avgFrameTime, scene->GetObjectCount(),
                                         sceneModeName, selectedObjName);

                renderer->BeginFrame();
                scene->Render();
                renderer->EndFrame();

                // Update overlay with collision + kernel timing diagnostics
                const auto& stats = scene->GetCollisionStats();

                FCL_DIAGNOSTICS_RESPONSE diag = {};
                bool diagOk = driver->QueryDiagnostics(diag);

                const double collisionAvgMs = (diag.Collision.CallCount > 0)
                    ? static_cast<double>(diag.Collision.TotalDurationMicroseconds) /
                          (1000.0 * static_cast<double>(diag.Collision.CallCount))
                    : 0.0;
                const double distanceAvgMs = (diag.Distance.CallCount > 0)
                    ? static_cast<double>(diag.Distance.TotalDurationMicroseconds) /
                          (1000.0 * static_cast<double>(diag.Distance.CallCount))
                    : 0.0;
                const double ccdAvgMs = (diag.ContinuousCollision.CallCount > 0)
                    ? static_cast<double>(diag.ContinuousCollision.TotalDurationMicroseconds) /
                          (1000.0 * static_cast<double>(diag.ContinuousCollision.CallCount))
                    : 0.0;

                wchar_t overlay[512];
                if (diagOk)
                {
                    swprintf_s(
                        overlay,
                        L"Collision Stats  |  Frames: %llu  |  LastPairs: %u  Hits: %u  |  TotalPairs: %llu  TotalHits: %llu\n"
                        L"Kernel Timing (avg, ms)  |  Collision: %.3f  Distance: %.3f  CCD: %.3f",
                        static_cast<unsigned long long>(stats.FrameCount),
                        stats.LastFramePairs,
                        stats.LastFrameHits,
                        static_cast<unsigned long long>(stats.TotalPairs),
                        static_cast<unsigned long long>(stats.TotalHits),
                        collisionAvgMs,
                        distanceAvgMs,
                        ccdAvgMs);
                }
                else
                {
                    swprintf_s(
                        overlay,
                        L"Collision Stats  |  Frames: %llu  |  LastPairs: %u  Hits: %u  |  TotalPairs: %llu  TotalHits: %llu\n"
                        L"Kernel Timing: unavailable (driver not connected or IOCTL failed)",
                        static_cast<unsigned long long>(stats.FrameCount),
                        stats.LastFramePairs,
                        stats.LastFrameHits,
                        static_cast<unsigned long long>(stats.TotalPairs),
                        static_cast<unsigned long long>(stats.TotalHits));
                }

                window->SetOverlayText(overlay);
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
