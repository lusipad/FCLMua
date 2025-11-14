#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <directxmath.h>

// FCL driver structures (from kernel headers)
#pragma pack(push, 1)

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

struct FCL_COLLISION_IO_BUFFER {
    struct {
        FCL_GEOMETRY_HANDLE Object1;
        FCL_TRANSFORM Transform1;
        FCL_GEOMETRY_HANDLE Object2;
        FCL_TRANSFORM Transform2;
    } Query;

    struct {
        UCHAR IsColliding;
        FCL_CONTACT_INFO Contact;
    } Result;
};

struct FCL_DISTANCE_RESULT {
    float Distance;
    FCL_VECTOR3 PointOnObject1;
    FCL_VECTOR3 PointOnObject2;
};

struct FCL_CREATE_SPHERE_BUFFER {
    FCL_VECTOR3 Center;
    float Radius;
    FCL_GEOMETRY_HANDLE Handle; // Output
};

struct FCL_CREATE_MESH_BUFFER {
    UINT32 VertexCount;
    UINT32 IndexCount;
    FCL_GEOMETRY_HANDLE Handle; // Output
    // Followed by: VertexCount * FCL_VECTOR3, then IndexCount * UINT32
};

#pragma pack(pop)

// IOCTL codes
#define IOCTL_FCL_PING                CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FCL_CREATE_SPHERE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FCL_CREATE_MESH         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x814, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FCL_DESTROY_GEOMETRY    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FCL_QUERY_COLLISION     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FCL_QUERY_DISTANCE      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)

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

    // Utility
    static FCL_TRANSFORM CreateTransform(const DirectX::XMFLOAT3& position,
                                          const DirectX::XMMATRIX& rotation);
    static FCL_VECTOR3 ToFclVector(const DirectX::XMFLOAT3& v);
    static DirectX::XMFLOAT3 FromFclVector(const FCL_VECTOR3& v);

private:
    HANDLE m_device;
};
