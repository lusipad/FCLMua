#include "fcl_driver.h"
#include <stdexcept>

using namespace DirectX;

FclDriver::FclDriver()
    : m_device(INVALID_HANDLE_VALUE)
    , m_r3Initialized(false)
    , m_backendMode(BackendMode::None)
{
}

FclDriver::~FclDriver()
{
    Disconnect();
    ShutdownR3();
}

bool FclDriver::Connect(const std::wstring& devicePath)
{
    if (IsR3Mode())
    {
        ShutdownR3();
    }

    Disconnect();

    m_device = CreateFileW(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (m_device != INVALID_HANDLE_VALUE)
    {
        m_backendMode = BackendMode::Driver;
        return true;
    }

    m_backendMode = BackendMode::None;
    return false;
}

void FclDriver::Disconnect()
{
    if (m_device != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_device);
        m_device = INVALID_HANDLE_VALUE;
    }

    if (m_backendMode == BackendMode::Driver && m_device == INVALID_HANDLE_VALUE)
    {
        m_backendMode = BackendMode::None;
    }
}

bool FclDriver::InitializeR3()
{
    if (m_backendMode == BackendMode::Driver)
    {
        Disconnect();
    }

    if (m_r3Initialized)
    {
        m_backendMode = BackendMode::R3;
        return true;
    }

    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status))
    {
        m_backendMode = BackendMode::None;
        return false;
    }

    m_r3Initialized = true;
    m_backendMode = BackendMode::R3;
    return true;
}

void FclDriver::ShutdownR3()
{
    if (m_r3Initialized)
    {
        FclGeometrySubsystemShutdown();
        m_r3Initialized = false;
    }

    if (m_backendMode == BackendMode::R3)
    {
        m_backendMode = BackendMode::None;
    }
}

bool FclDriver::IsReady() const
{
    if (IsDriverMode())
    {
        return true;
    }
    if (IsR3Mode())
    {
        return true;
    }
    return false;
}

std::wstring FclDriver::GetBackendDisplayName() const
{
    switch (m_backendMode)
    {
    case BackendMode::Driver:
        return IsConnected() ? L"Kernel Driver (R0)" : L"Kernel Driver (disconnected)";
    case BackendMode::R3:
        return m_r3Initialized ? L"R3 (User Mode)" : L"R3 (uninitialized)";
    default:
        return L"Unavailable";
    }
}

FCL_GEOMETRY_HANDLE FclDriver::CreateSphere(const XMFLOAT3& center, float radius)
{
    if (!IsReady())
        return FCL_GEOMETRY_HANDLE{ 0 };

    if (IsDriverMode())
    {
        FCL_CREATE_SPHERE_INPUT input = {};
        input.Desc.Center = ToFclVector(center);
        input.Desc.Radius = radius;

        FCL_CREATE_SPHERE_OUTPUT output = {};

        DWORD bytesReturned = 0;
        BOOL success = DeviceIoControl(
            m_device,
            IOCTL_FCL_CREATE_SPHERE,
            &input,
            static_cast<DWORD>(sizeof(input)),
            &output,
            static_cast<DWORD>(sizeof(output)),
            &bytesReturned,
            nullptr
        );

        if (!success)
            return FCL_GEOMETRY_HANDLE{ 0 };

        return output.Handle;
    }

    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = ToFclVector(center);
    desc.Radius = radius;
    FCL_GEOMETRY_HANDLE handle = {};
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &handle);
    if (!NT_SUCCESS(status))
        return FCL_GEOMETRY_HANDLE{ 0 };
    return handle;
}

FCL_GEOMETRY_HANDLE FclDriver::CreateBox(const XMFLOAT3& center, const XMFLOAT3& extents)
{
    UNREFERENCED_PARAMETER(center);

    // Represent box as a mesh centered at origin; translation/rotation are provided
    // per-query via FCL_TRANSFORM in collision/distance calls.
    std::vector<XMFLOAT3> vertices(8);

    const float ex = extents.x;
    const float ey = extents.y;
    const float ez = extents.z;

    // 8 corners of axis-aligned box around origin
    vertices[0] = XMFLOAT3(-ex, -ey, -ez);
    vertices[1] = XMFLOAT3(ex, -ey, -ez);
    vertices[2] = XMFLOAT3(ex, ey, -ez);
    vertices[3] = XMFLOAT3(-ex, ey, -ez);
    vertices[4] = XMFLOAT3(-ex, -ey, ez);
    vertices[5] = XMFLOAT3(ex, -ey, ez);
    vertices[6] = XMFLOAT3(ex, ey, ez);
    vertices[7] = XMFLOAT3(-ex, ey, ez);

    // 12 triangles (two per face)
    static const uint32_t indicesArray[] = {
        // Front (-Z)
        0, 1, 2, 0, 2, 3,
        // Back (+Z)
        4, 5, 6, 4, 6, 7,
        // Left (-X)
        0, 3, 7, 0, 7, 4,
        // Right (+X)
        1, 2, 6, 1, 6, 5,
        // Bottom (-Y)
        0, 1, 5, 0, 5, 4,
        // Top (+Y)
        3, 2, 6, 3, 6, 7,
    };

    std::vector<uint32_t> indices(std::begin(indicesArray), std::end(indicesArray));

    return CreateMesh(vertices, indices);
}

FCL_GEOMETRY_HANDLE FclDriver::CreateMesh(const std::vector<XMFLOAT3>& vertices,
                                           const std::vector<uint32_t>& indices)
{
    if (!IsReady())
        return FCL_GEOMETRY_HANDLE{ 0 };

    if (IsDriverMode())
    {
        // Calculate buffer size
        const size_t headerSize = sizeof(FCL_CREATE_MESH_BUFFER);
        size_t bufferSize = headerSize +
                            vertices.size() * sizeof(FCL_VECTOR3) +
                            indices.size() * sizeof(uint32_t);

        std::vector<BYTE> buffer(bufferSize);
        auto* meshBuffer = reinterpret_cast<FCL_CREATE_MESH_BUFFER*>(buffer.data());

        meshBuffer->VertexCount = static_cast<UINT32>(vertices.size());
        meshBuffer->IndexCount = static_cast<UINT32>(indices.size());
        meshBuffer->Reserved0 = 0;
        meshBuffer->Reserved1 = 0;

        // Copy vertices
        auto* vertexData = reinterpret_cast<FCL_VECTOR3*>(meshBuffer + 1);
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            vertexData[i] = ToFclVector(vertices[i]);
        }

        // Copy indices
        auto* indexData = reinterpret_cast<uint32_t*>(vertexData + vertices.size());
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

    std::vector<FCL_VECTOR3> converted(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        converted[i] = ToFclVector(vertices[i]);
    }

    FCL_MESH_GEOMETRY_DESC desc = {};
    desc.Vertices = converted.data();
    desc.VertexCount = static_cast<ULONG>(converted.size());
    desc.Indices = indices.data();
    desc.IndexCount = static_cast<ULONG>(indices.size());

    FCL_GEOMETRY_HANDLE handle = {};
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, &handle);
    if (!NT_SUCCESS(status))
        return FCL_GEOMETRY_HANDLE{ 0 };

    return handle;
}

bool FclDriver::DestroyGeometry(FCL_GEOMETRY_HANDLE handle)
{
    if (!IsReady())
        return false;

    if (IsDriverMode())
    {
        FCL_DESTROY_INPUT input = {};
        input.Handle = handle;

        DWORD bytesReturned = 0;
        BOOL success = DeviceIoControl(
            m_device,
            IOCTL_FCL_DESTROY_GEOMETRY,
            &input,
            static_cast<DWORD>(sizeof(input)),
            nullptr,
            0,
            &bytesReturned,
            nullptr
        );

        return success != FALSE;
    }

    NTSTATUS status = FclDestroyGeometry(handle);
    return NT_SUCCESS(status);
}

bool FclDriver::QueryCollision(FCL_GEOMETRY_HANDLE obj1, const FCL_TRANSFORM& transform1,
                                FCL_GEOMETRY_HANDLE obj2, const FCL_TRANSFORM& transform2,
                                bool& isColliding, FCL_CONTACT_INFO& contactInfo)
{
    if (!IsReady())
        return false;

    if (IsDriverMode())
    {
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
            static_cast<DWORD>(sizeof(buffer)),
            &buffer,
            static_cast<DWORD>(sizeof(buffer)),
            &bytesReturned,
            nullptr
        );

        if (!success)
            return false;

        isColliding = buffer.Result.IsColliding != 0;
        contactInfo = buffer.Result.Contact;
        return true;
    }

    BOOLEAN intersecting = FALSE;
    FCL_CONTACT_INFO info = {};
    NTSTATUS status = FclCollisionDetect(
        obj1,
        &transform1,
        obj2,
        &transform2,
        &intersecting,
        &info);
    if (!NT_SUCCESS(status))
        return false;

    isColliding = intersecting != FALSE;
    contactInfo = info;
    return true;
}

bool FclDriver::QueryDistance(FCL_GEOMETRY_HANDLE obj1, const FCL_TRANSFORM& transform1,
                               FCL_GEOMETRY_HANDLE obj2, const FCL_TRANSFORM& transform2,
                               FCL_DISTANCE_RESULT& result)
{
    if (!IsReady())
        return false;

    if (IsDriverMode())
    {
        FCL_DISTANCE_IO_BUFFER buffer = {};
        buffer.Query.Object1 = obj1;
        buffer.Query.Transform1 = transform1;
        buffer.Query.Object2 = obj2;
        buffer.Query.Transform2 = transform2;

        DWORD bytesReturned = 0;
        BOOL success = DeviceIoControl(
            m_device,
            IOCTL_FCL_QUERY_DISTANCE,
            &buffer,
            static_cast<DWORD>(sizeof(buffer)),
            &buffer,
            static_cast<DWORD>(sizeof(buffer)),
            &bytesReturned,
            nullptr
        );

        if (!success)
            return false;

        result.Distance = buffer.Result.Distance;
        result.ClosestPoint1 = buffer.Result.ClosestPoint1;
        result.ClosestPoint2 = buffer.Result.ClosestPoint2;
        return true;
    }

    FCL_DISTANCE_RESULT local = {};
    NTSTATUS status = FclDistanceCompute(
        obj1,
        &transform1,
        obj2,
        &transform2,
        &local);
    if (!NT_SUCCESS(status))
        return false;

    result = local;
    return true;
}

bool FclDriver::QueryDiagnostics(FCL_DIAGNOSTICS_RESPONSE& diagnostics)
{
    if (!IsDriverMode())
        return false;

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        m_device,
        IOCTL_FCL_QUERY_DIAGNOSTICS,
        nullptr,
        0,
        &diagnostics,
        static_cast<DWORD>(sizeof(diagnostics)),
        &bytesReturned,
        nullptr
    );

    return success != FALSE;
}

FCL_TRANSFORM FclDriver::CreateTransform(const XMFLOAT3& position, const XMMATRIX& rotation, const XMFLOAT3& scale)
{
    FCL_TRANSFORM transform = {};
    transform.Translation = ToFclVector(position);

    // Combine scale and rotation matrices
    // FCL_TRANSFORM only has rotation matrix, so we bake the scale into it
    XMMATRIX scaleMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX scaledRotation = scaleMatrix * rotation;

    // Extract the combined scale+rotation matrix (3x3 from 4x4)
    XMFLOAT4X4 sr4x4;
    XMStoreFloat4x4(&sr4x4, scaledRotation);

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            transform.Rotation.M[i][j] = sr4x4.m[i][j];
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

bool FclDriver::UpdateTransform(FCL_GEOMETRY_HANDLE handle, const FCL_TRANSFORM& transform)
{
    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(transform);
    // Current driver API is stateless with respect to transforms; they are provided
    // per-query in collision/distance requests. This is a no-op kept for API compatibility.
    return true;
}
