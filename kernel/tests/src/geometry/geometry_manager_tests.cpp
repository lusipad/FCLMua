#include <ntddk.h>
#include <wdm.h>

#include "fclmusa/geometry.h"
#include "fclmusa/geometry/geometry_tests.h"
#include "fclmusa/logging.h"
#include "fclmusa/test/assertions.h"

namespace {

constexpr float kFloatTolerance = 1e-4f;

const FCL_VECTOR3 kMeshVerticesInitial[] = {
    {0.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
};

const FCL_VECTOR3 kMeshVerticesUpdated[] = {
    {0.0f, 0.0f, 0.0f},
    {2.0f, 0.0f, 0.0f},
    {0.0f, 2.0f, 0.0f},
    {0.0f, 0.0f, 2.0f},
};

const UINT32 kMeshIndices[] = {
    0, 1, 2,
    0, 1, 3,
    0, 2, 3,
    1, 2, 3,
};

FCL_SPHERE_GEOMETRY_DESC MakeSphereDesc(float radius) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {1.0f, 2.0f, 3.0f};
    desc.Radius = radius;
    return desc;
}

FCL_MESH_GEOMETRY_DESC MakeMeshDesc(const FCL_VECTOR3* vertices, ULONG vertexCount) noexcept {
    FCL_MESH_GEOMETRY_DESC desc = {};
    desc.Vertices = vertices;
    desc.VertexCount = vertexCount;
    desc.Indices = kMeshIndices;
    desc.IndexCount = RTL_NUMBER_OF(kMeshIndices);
    return desc;
}

NTSTATUS RunSphereCreationTests() noexcept {
    FCL_GEOMETRY_HANDLE handle = {};

    FCL_TEST_EXPECT_STATUS(
        FclCreateGeometry(FCL_GEOMETRY_SPHERE, nullptr, &handle),
        STATUS_INVALID_PARAMETER);

    auto invalidDesc = MakeSphereDesc(-1.0f);
    FCL_TEST_EXPECT_STATUS(
        FclCreateGeometry(FCL_GEOMETRY_SPHERE, &invalidDesc, &handle),
        STATUS_INVALID_PARAMETER);

    auto validDesc = MakeSphereDesc(2.5f);
    FCL_TEST_EXPECT_NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_SPHERE, &validDesc, &handle));
    FCL_TEST_EXPECT_TRUE(FclIsGeometryHandleValid(handle), STATUS_INTERNAL_ERROR);

    FCL_TEST_EXPECT_NT_SUCCESS(FclDestroyGeometry(handle));
    FCL_TEST_EXPECT_STATUS(FclDestroyGeometry(handle), STATUS_INVALID_HANDLE);

    return STATUS_SUCCESS;
}

NTSTATUS RunReferenceGuardTests() noexcept {
    auto desc = MakeSphereDesc(1.25f);
    FCL_GEOMETRY_HANDLE handle = {};
    FCL_TEST_EXPECT_NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &handle));

    FCL_GEOMETRY_REFERENCE reference = {};
    FCL_GEOMETRY_SNAPSHOT snapshot = {};
    FCL_TEST_EXPECT_NT_SUCCESS(FclAcquireGeometryReference(handle, &reference, &snapshot));

    FCL_TEST_EXPECT_TRUE(reference.HandleValue == handle.Value, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(snapshot.Type == FCL_GEOMETRY_SPHERE, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_FLOAT_NEAR(
        snapshot.Data.Sphere.Radius,
        desc.Radius,
        kFloatTolerance,
        STATUS_DATA_ERROR);

    FCL_TEST_EXPECT_STATUS(FclDestroyGeometry(handle), STATUS_DEVICE_BUSY);

    FclReleaseGeometryReference(&reference);
    FCL_TEST_EXPECT_TRUE(reference.HandleValue == 0, STATUS_DATA_ERROR);

    FCL_TEST_EXPECT_NT_SUCCESS(FclDestroyGeometry(handle));

    FCL_GEOMETRY_REFERENCE invalidReference = {};
    FCL_TEST_EXPECT_STATUS(
        FclAcquireGeometryReference({0}, &invalidReference, nullptr),
        STATUS_INVALID_HANDLE);

    return STATUS_SUCCESS;
}

NTSTATUS RunMeshUpdateTests() noexcept {
    const auto initialDesc = MakeMeshDesc(kMeshVerticesInitial, RTL_NUMBER_OF(kMeshVerticesInitial));
    const auto updatedDesc = MakeMeshDesc(kMeshVerticesUpdated, RTL_NUMBER_OF(kMeshVerticesUpdated));

    FCL_GEOMETRY_HANDLE meshHandle = {};
    FCL_TEST_EXPECT_NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_MESH, &initialDesc, &meshHandle));

    FCL_GEOMETRY_REFERENCE meshRef = {};
    FCL_GEOMETRY_SNAPSHOT snapshot = {};

    FCL_TEST_EXPECT_NT_SUCCESS(FclAcquireGeometryReference(meshHandle, &meshRef, &snapshot));
    FCL_TEST_EXPECT_TRUE(snapshot.Type == FCL_GEOMETRY_MESH, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(
        snapshot.Data.Mesh.VertexCount == RTL_NUMBER_OF(kMeshVerticesInitial),
        STATUS_DATA_ERROR);

    FCL_TEST_EXPECT_STATUS(FclUpdateMeshGeometry(meshHandle, &updatedDesc), STATUS_DEVICE_BUSY);

    FclReleaseGeometryReference(&meshRef);
    FCL_TEST_EXPECT_TRUE(meshRef.HandleValue == 0, STATUS_DATA_ERROR);

    FCL_TEST_EXPECT_NT_SUCCESS(FclUpdateMeshGeometry(meshHandle, &updatedDesc));

    FCL_TEST_EXPECT_NT_SUCCESS(FclAcquireGeometryReference(meshHandle, &meshRef, &snapshot));
    FCL_TEST_EXPECT_TRUE(
        snapshot.Data.Mesh.VertexCount == RTL_NUMBER_OF(kMeshVerticesUpdated),
        STATUS_DATA_ERROR);
    FclReleaseGeometryReference(&meshRef);

    FCL_TEST_EXPECT_NT_SUCCESS(FclDestroyGeometry(meshHandle));

    auto sphereDesc = MakeSphereDesc(3.0f);
    FCL_GEOMETRY_HANDLE sphereHandle = {};
    FCL_TEST_EXPECT_NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphereDesc, &sphereHandle));
    FCL_TEST_EXPECT_STATUS(FclUpdateMeshGeometry(sphereHandle, &updatedDesc), STATUS_NOT_SUPPORTED);
    FCL_TEST_EXPECT_NT_SUCCESS(FclDestroyGeometry(sphereHandle));

    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunGeometryManagerTests() noexcept {
    const NTSTATUS initStatus = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(initStatus)) {
        return initStatus;
    }

    FCL_TEST_EXPECT_NT_SUCCESS(RunSphereCreationTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunReferenceGuardTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunMeshUpdateTests());

    return STATUS_SUCCESS;
}

