#include "fcl_driver.h"
#include <stdexcept>

using namespace DirectX;

FclDriver::FclDriver()
    : m_device(INVALID_HANDLE_VALUE)
{
}

FclDriver::~FclDriver()
{
    Disconnect();
}

bool FclDriver::Connect(const std::wstring& devicePath)
{
    m_device = CreateFileW(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    return m_device != INVALID_HANDLE_VALUE;
}

void FclDriver::Disconnect()
{
    if (m_device != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_device);
        m_device = INVALID_HANDLE_VALUE;
    }
}

FCL_GEOMETRY_HANDLE FclDriver::CreateSphere(const XMFLOAT3& center, float radius)
{
    if (!IsConnected())
        return FCL_GEOMETRY_HANDLE{ 0 };

    FCL_CREATE_SPHERE_BUFFER buffer = {};
    buffer.Center = ToFclVector(center);
    buffer.Radius = radius;

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        m_device,
        IOCTL_FCL_CREATE_SPHERE,
        &buffer,
        sizeof(buffer),
        &buffer,
        sizeof(buffer),
        &bytesReturned,
        nullptr
    );

    if (!success)
        return FCL_GEOMETRY_HANDLE{ 0 };

    return buffer.Handle;
}

FCL_GEOMETRY_HANDLE FclDriver::CreateMesh(const std::vector<XMFLOAT3>& vertices,
                                           const std::vector<uint32_t>& indices)
{
    if (!IsConnected())
        return FCL_GEOMETRY_HANDLE{ 0 };

    // Calculate buffer size
    size_t bufferSize = sizeof(FCL_CREATE_MESH_BUFFER) +
                        vertices.size() * sizeof(FCL_VECTOR3) +
                        indices.size() * sizeof(uint32_t);

    std::vector<BYTE> buffer(bufferSize);
    FCL_CREATE_MESH_BUFFER* meshBuffer = reinterpret_cast<FCL_CREATE_MESH_BUFFER*>(buffer.data());

    meshBuffer->VertexCount = static_cast<UINT32>(vertices.size());
    meshBuffer->IndexCount = static_cast<UINT32>(indices.size());

    // Copy vertices
    FCL_VECTOR3* vertexData = reinterpret_cast<FCL_VECTOR3*>(meshBuffer + 1);
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        vertexData[i] = ToFclVector(vertices[i]);
    }

    // Copy indices
    uint32_t* indexData = reinterpret_cast<uint32_t*>(vertexData + vertices.size());
    memcpy(indexData, indices.data(), indices.size() * sizeof(uint32_t));

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        m_device,
        IOCTL_FCL_CREATE_MESH,
        buffer.data(),
        static_cast<DWORD>(bufferSize),
        buffer.data(),
        static_cast<DWORD>(bufferSize),
        &bytesReturned,
        nullptr
    );

    if (!success)
        return FCL_GEOMETRY_HANDLE{ 0 };

    return meshBuffer->Handle;
}

bool FclDriver::DestroyGeometry(FCL_GEOMETRY_HANDLE handle)
{
    if (!IsConnected())
        return false;

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        m_device,
        IOCTL_FCL_DESTROY_GEOMETRY,
        &handle,
        sizeof(handle),
        nullptr,
        0,
        &bytesReturned,
        nullptr
    );

    return success != FALSE;
}

bool FclDriver::QueryCollision(FCL_GEOMETRY_HANDLE obj1, const FCL_TRANSFORM& transform1,
                                FCL_GEOMETRY_HANDLE obj2, const FCL_TRANSFORM& transform2,
                                bool& isColliding, FCL_CONTACT_INFO& contactInfo)
{
    if (!IsConnected())
        return false;

    FCL_COLLISION_IO_BUFFER buffer = {};
    buffer.Query.Object1 = obj1;
    buffer.Query.Transform1 = transform1;
    buffer.Query.Object2 = obj2;
    buffer.Query.Transform2 = transform2;

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        m_device,
        IOCTL_FCL_QUERY_COLLISION,
        &buffer,
        sizeof(buffer),
        &buffer,
        sizeof(buffer),
        &bytesReturned,
        nullptr
    );

    if (!success)
        return false;

    isColliding = buffer.Result.IsColliding != 0;
    contactInfo = buffer.Result.Contact;
    return true;
}

bool FclDriver::QueryDistance(FCL_GEOMETRY_HANDLE obj1, const FCL_TRANSFORM& transform1,
                               FCL_GEOMETRY_HANDLE obj2, const FCL_TRANSFORM& transform2,
                               FCL_DISTANCE_RESULT& result)
{
    if (!IsConnected())
        return false;

    struct {
        struct {
            FCL_GEOMETRY_HANDLE Object1;
            FCL_TRANSFORM Transform1;
            FCL_GEOMETRY_HANDLE Object2;
            FCL_TRANSFORM Transform2;
        } Query;
        FCL_DISTANCE_RESULT Result;
    } buffer = {};

    buffer.Query.Object1 = obj1;
    buffer.Query.Transform1 = transform1;
    buffer.Query.Object2 = obj2;
    buffer.Query.Transform2 = transform2;

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        m_device,
        IOCTL_FCL_QUERY_DISTANCE,
        &buffer,
        sizeof(buffer),
        &buffer,
        sizeof(buffer),
        &bytesReturned,
        nullptr
    );

    if (!success)
        return false;

    result = buffer.Result;
    return true;
}

FCL_TRANSFORM FclDriver::CreateTransform(const XMFLOAT3& position, const XMMATRIX& rotation)
{
    FCL_TRANSFORM transform = {};
    transform.Translation = ToFclVector(position);

    // Extract rotation matrix (3x3 from 4x4)
    XMFLOAT4X4 rot4x4;
    XMStoreFloat4x4(&rot4x4, rotation);

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            transform.Rotation.M[i][j] = rot4x4.m[i][j];
        }
    }

    return transform;
}

FCL_VECTOR3 FclDriver::ToFclVector(const XMFLOAT3& v)
{
    FCL_VECTOR3 result;
    result.X = v.x;
    result.Y = v.y;
    result.Z = v.z;
    return result;
}

XMFLOAT3 FclDriver::FromFclVector(const FCL_VECTOR3& v)
{
    return XMFLOAT3(v.X, v.Y, v.Z);
}
