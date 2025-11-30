#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <directxmath.h>

struct FCL_VECTOR3 {
    float X, Y, Z;
};

struct FCL_MATRIX3X3 {
    float M[3][3];
};

struct FCL_TRANSFORM {
    FCL_MATRIX3X3 Rotation;
    FCL_VECTOR3 Translation;
};

struct FCL_GEOMETRY_HANDLE {
    ULONGLONG Value;
};

struct FCL_CONTACT_INFO {
    FCL_VECTOR3 PointOnObject1;
    FCL_VECTOR3 PointOnObject2;
    FCL_VECTOR3 Normal;
    float PenetrationDepth;
};

struct FCL_COLLISION_QUERY {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM Transform2;
};

struct FCL_DISTANCE_RESULT {
    float Distance;
    FCL_VECTOR3 ClosestPoint1;
    FCL_VECTOR3 ClosestPoint2;
};

struct FCL_COLLISION_RESULT {
    UCHAR IsColliding;
    UCHAR Reserved[3];
    FCL_CONTACT_INFO Contact;
};

struct FCL_COLLISION_IO_BUFFER {
    FCL_COLLISION_QUERY Query;
    FCL_COLLISION_RESULT Result;
};

struct FCL_DISTANCE_QUERY {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM Transform2;
};

struct FCL_DISTANCE_IO_BUFFER {
    FCL_DISTANCE_QUERY Query;
    FCL_DISTANCE_RESULT Result;
};

struct FCL_SPHERE_GEOMETRY_DESC {
    FCL_VECTOR3 Center;
    float Radius;
};

struct FCL_CREATE_SPHERE_INPUT {
    FCL_SPHERE_GEOMETRY_DESC Desc;
};

struct FCL_CREATE_SPHERE_OUTPUT {
    FCL_GEOMETRY_HANDLE Handle;
};

struct FCL_DESTROY_INPUT {
    FCL_GEOMETRY_HANDLE Handle;
};

struct FCL_CREATE_MESH_BUFFER {
    UINT32 VertexCount;
    UINT32 IndexCount;
    UINT32 Reserved0;
    UINT32 Reserved1;
    FCL_GEOMETRY_HANDLE Handle; // Output
    // Followed by: VertexCount * FCL_VECTOR3, then IndexCount * UINT32
};

struct FCL_DETECTION_TIMING_STATS {
    ULONGLONG CallCount;
    ULONGLONG TotalDurationMicroseconds;
    ULONGLONG MinDurationMicroseconds;
    ULONGLONG MaxDurationMicroseconds;
};

struct FCL_DIAGNOSTICS_RESPONSE {
    FCL_DETECTION_TIMING_STATS Collision;
    FCL_DETECTION_TIMING_STATS Distance;
    FCL_DETECTION_TIMING_STATS ContinuousCollision;
    FCL_DETECTION_TIMING_STATS DpcCollision;
};

// IOCTL codes (must match r0/core/include/fclmusa/ioctl.h)
#define IOCTL_FCL_PING                CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_DIAGNOSTICS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_SPHERE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_MESH         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x814, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_DESTROY_GEOMETRY    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_COLLISION     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_DISTANCE      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

class FclDriver
{
public:
    FclDriver();
    ~FclDriver();

    // Connection
    bool Connect(const std::wstring& devicePath);
    void Disconnect();
    bool IsConnected() const { return m_device != INVALID_HANDLE_VALUE; }

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
};
