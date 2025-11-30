#pragma once

#ifndef WIN32_NO_STATUS
#define WIN32_NO_STATUS
#define FCL_GUI_DEMO_DEFINED_WIN32_NO_STATUS
#endif
#include <windows.h>
#ifdef FCL_GUI_DEMO_DEFINED_WIN32_NO_STATUS
#undef WIN32_NO_STATUS
#undef FCL_GUI_DEMO_DEFINED_WIN32_NO_STATUS
#endif

#include <directxmath.h>
#include <string>
#include <vector>

#include "fclmusa/collision.h"
#include "fclmusa/distance.h"
#include "fclmusa/geometry.h"
#include "fclmusa/ioctl.h"

class FclDriver
{
public:
    enum class BackendMode
    {
        None,
        Driver,
        R3
    };

    FclDriver();
    ~FclDriver();

    // Connection
    bool Connect(const std::wstring& devicePath);
    void Disconnect();
    bool IsConnected() const { return m_device != INVALID_HANDLE_VALUE; }
    bool InitializeR3();
    void ShutdownR3();
    bool IsReady() const;
    BackendMode GetBackendMode() const { return m_backendMode; }
    bool IsR3Mode() const { return m_backendMode == BackendMode::R3 && m_r3Initialized; }
    bool IsDriverMode() const { return m_backendMode == BackendMode::Driver && IsConnected(); }
    std::wstring GetBackendDisplayName() const;

    // Geometry creation
    FCL_GEOMETRY_HANDLE CreateSphere(const DirectX::XMFLOAT3& center, float radius);
    FCL_GEOMETRY_HANDLE CreateBox(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& extents);
    FCL_GEOMETRY_HANDLE CreateMesh(const std::vector<DirectX::XMFLOAT3>& vertices,
                                    const std::vector<uint32_t>& indices);
    bool DestroyGeometry(FCL_GEOMETRY_HANDLE handle);

    // Collision queries
    bool QueryCollision(FCL_GEOMETRY_HANDLE obj1, const FCL_TRANSFORM& transform1,
                        FCL_GEOMETRY_HANDLE obj2, const FCL_TRANSFORM& transform2,
                        bool& isColliding, FCL_CONTACT_INFO& contactInfo);

    bool QueryDistance(FCL_GEOMETRY_HANDLE obj1, const FCL_TRANSFORM& transform1,
                       FCL_GEOMETRY_HANDLE obj2, const FCL_TRANSFORM& transform2,
                       FCL_DISTANCE_RESULT& result);

    // Transform updates (no-op in current thin driver, kept for API compatibility)
    bool UpdateTransform(FCL_GEOMETRY_HANDLE handle, const FCL_TRANSFORM& transform);

    // Utility
    static FCL_TRANSFORM CreateTransform(const DirectX::XMFLOAT3& position,
                                          const DirectX::XMMATRIX& rotation,
                                          const DirectX::XMFLOAT3& scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
    static FCL_VECTOR3 ToFclVector(const DirectX::XMFLOAT3& v);
    static DirectX::XMFLOAT3 FromFclVector(const FCL_VECTOR3& v);

    bool QueryDiagnostics(FCL_DIAGNOSTICS_RESPONSE& diagnostics);

private:
    HANDLE m_device;
    bool m_r3Initialized;
    BackendMode m_backendMode;
};
