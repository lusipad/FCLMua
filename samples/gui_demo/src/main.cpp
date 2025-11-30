// FCL Collision Demo - Windows GUI Application
// 3D visualization and collision detection demo with interactive wizard

#ifndef WIN32_NO_STATUS
#define WIN32_NO_STATUS
#define FCL_GUI_MAIN_DEFINED_WIN32_NO_STATUS
#endif
#include <windows.h>
#ifdef FCL_GUI_MAIN_DEFINED_WIN32_NO_STATUS
#undef WIN32_NO_STATUS
#undef FCL_GUI_MAIN_DEFINED_WIN32_NO_STATUS
#endif
#include <shellapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <algorithm>
#include <cwctype>
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

namespace
{
enum class BackendPreference
{
    Auto,
    DriverOnly,
    R3Only
};

std::wstring ToLower(std::wstring text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return text;
}

BackendPreference ParseBackendPreferenceFromArgs()
{
    BackendPreference preference = BackendPreference::Auto;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
    {
        return preference;
    }

    for (int i = 1; i < argc; ++i)
    {
        std::wstring lower = ToLower(std::wstring(argv[i]));
        if (lower == L"--mode=r3" || lower == L"/mode=r3" ||
            lower == L"--r3" || lower == L"/r3" || lower == L"-r3" ||
            lower == L"--use-r3")
        {
            preference = BackendPreference::R3Only;
        }
        else if (lower == L"--mode=driver" || lower == L"/mode=driver" ||
                 lower == L"--driver" || lower == L"/driver" ||
                 lower == L"-driver" || lower == L"--use-driver" ||
                 lower == L"--mode=r0")
        {
            preference = BackendPreference::DriverOnly;
        }
    }

    LocalFree(argv);
    return preference;
}

BackendPreference ParseBackendPreferenceFromEnv()
{
    wchar_t buffer[32] = {};
    DWORD length = GetEnvironmentVariableW(L"FCL_GUI_BACKEND", buffer, static_cast<DWORD>(_countof(buffer)));
    if (length == 0 || length >= _countof(buffer))
    {
        return BackendPreference::Auto;
    }

    std::wstring lower = ToLower(std::wstring(buffer, length));
    if (lower == L"r3" || lower == L"user")
    {
        return BackendPreference::R3Only;
    }
    if (lower == L"driver" || lower == L"r0" || lower == L"kernel")
    {
        return BackendPreference::DriverOnly;
    }
    return BackendPreference::Auto;
}

BackendPreference ResolveBackendPreference()
{
    BackendPreference preference = ParseBackendPreferenceFromArgs();
    if (preference != BackendPreference::Auto)
    {
        return preference;
    }
    preference = ParseBackendPreferenceFromEnv();
    return preference;
}
} // namespace

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

        const BackendPreference backendPreference = ResolveBackendPreference();

        // Initialize collision backend
        auto driver = std::make_unique<FclDriver>();
        bool backendReady = false;

        auto showDriverInstructions = []() {
            MessageBoxA(nullptr,
                "Failed to connect to the FCL driver.\n\n"
                "Make sure the kernel driver is loaded (sc start FclMusa)\n"
                "or run the GUI with Administrator privileges.",
                "Driver Connection Error", MB_OK | MB_ICONWARNING);
        };

        auto showR3Instructions = []() {
            MessageBoxA(nullptr,
                "Failed to initialize the user-mode R3 backend.\n\n"
                "Build the user-mode library first:\n"
                "  pwsh -File tools\\build\\build-tasks.ps1 -Task R3-Lib-Release",
                "R3 Backend Error", MB_OK | MB_ICONWARNING);
        };

        switch (backendPreference)
        {
        case BackendPreference::DriverOnly:
            backendReady = driver->Connect(L"\\\\.\\FclMusa");
            if (!backendReady)
            {
                showDriverInstructions();
            }
            break;
        case BackendPreference::R3Only:
            backendReady = driver->InitializeR3();
            if (!backendReady)
            {
                showR3Instructions();
            }
            break;
        case BackendPreference::Auto:
        default:
            backendReady = driver->Connect(L"\\\\.\\FclMusa");
            if (!backendReady)
            {
                backendReady = driver->InitializeR3();
                if (backendReady)
                {
                    MessageBoxA(nullptr,
                        "Kernel driver unavailable. Collision detection will run with the user-mode R3 backend.",
                        "Switched to R3 backend", MB_OK | MB_ICONINFORMATION);
                }
                else
                {
                    showDriverInstructions();
                    showR3Instructions();
                }
            }
            break;
        }

        std::wstring backendStatus = L"Collision backend: " + driver->GetBackendDisplayName();
        if (!backendReady)
        {
            backendStatus += L" (collision disabled)";
        }
        window->SetStatusText(backendStatus);

        // Create scene
        auto scene = std::make_unique<Scene>(renderer.get(), driver.get());

        // Connect Window resize to Renderer resize
        window->OnResize = [&renderer](int width, int height) {
            if (renderer)
            {
                renderer->Resize(width, height);
            }
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
        float uiUpdateTimer = 0.0f;
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
                uiUpdateTimer += deltaTime;

                // Update FPS calculation every 0.5 seconds
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

                renderer->BeginFrame();
                scene->Render();
                renderer->EndFrame();

                // Update UI at 10Hz to prevent flickering of transparent overlays
                if (uiUpdateTimer >= 0.1f)
                {
                    uiUpdateTimer = 0.0f;

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

                    // Update overlay with collision + kernel timing diagnostics
                    const auto& stats = scene->GetCollisionStats();

                    FCL_DIAGNOSTICS_RESPONSE diag = {};
                    bool diagOk = driver->QueryDiagnostics(diag);
                    const std::wstring backendLabel = driver->GetBackendDisplayName();

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
                            L"Backend: %s\n"
                            L"Collision Stats  |  Frames: %llu  |  LastPairs: %u  Hits: %u  |  TotalPairs: %llu  TotalHits: %llu\n"
                            L"Kernel Timing (avg, ms)  |  Collision: %.3f  Distance: %.3f  CCD: %.3f",
                            backendLabel.c_str(),
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
                        const wchar_t* diagMessage = driver->IsR3Mode()
                            ? L"Kernel Timing: unavailable (R3 backend)"
                            : L"Kernel Timing: unavailable (driver not connected or IOCTL failed)";
                        swprintf_s(
                            overlay,
                            L"Backend: %s\n"
                            L"Collision Stats  |  Frames: %llu  |  LastPairs: %u  Hits: %u  |  TotalPairs: %llu  TotalHits: %llu\n"
                            L"%s",
                            backendLabel.c_str(),
                            static_cast<unsigned long long>(stats.FrameCount),
                            stats.LastFramePairs,
                            stats.LastFrameHits,
                            static_cast<unsigned long long>(stats.TotalPairs),
                            static_cast<unsigned long long>(stats.TotalHits),
                            diagMessage);
                    }

                    window->SetOverlayText(overlay);
                }
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
