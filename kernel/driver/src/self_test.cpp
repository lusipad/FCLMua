#include <ntddk.h>
#include <wdm.h>

#include <cmath>

#include "fclmusa/broadphase.h"
#include "fclmusa/collision.h"
#include "fclmusa/distance.h"
#include "fclmusa/driver.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/logging.h"
#include "fclmusa/memory/pool_allocator.h"
#include "fclmusa/self_test.h"

namespace {

using namespace fclmusa::geom;

constexpr float kCollisionSeparation = 0.5f;

NTSTATUS CreateSphere(float radius, _Out_ PFCL_GEOMETRY_HANDLE handle) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = radius;
    return FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, handle);
}

NTSTATUS DestroyHandle(_Inout_ PFCL_GEOMETRY_HANDLE handle) noexcept {
    if (handle == nullptr || handle->Value == 0) {
        return STATUS_SUCCESS;
    }
    const NTSTATUS status = FclDestroyGeometry(*handle);
    handle->Value = 0;
    return status;
}

ULONGLONG AbsoluteDifference(ULONGLONG a, ULONGLONG b) noexcept {
    return (a > b) ? (a - b) : (b - a);
}

ULONGLONG QueryTimeMicroseconds() noexcept {
    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};
    KeQueryPerformanceCounter(&counter);
    KeQueryPerformanceCounter(&frequency);
    if (frequency.QuadPart == 0) {
        return 0;
    }
    return static_cast<ULONGLONG>((counter.QuadPart * 1'000'000) / frequency.QuadPart);
}

void CopyContactSummary(_Out_ PFCL_CONTACT_SUMMARY summary, const FCL_CONTACT_INFO& contact) noexcept {
    if (summary == nullptr) {
        return;
    }
    summary->PointOnObject1 = contact.PointOnObject1;
    summary->PointOnObject2 = contact.PointOnObject2;
    summary->Normal = contact.Normal;
    summary->PenetrationDepth = contact.PenetrationDepth;
}

NTSTATUS CreateUnitMesh(FCL_GEOMETRY_HANDLE* handle) noexcept {
    static const FCL_VECTOR3 vertices[] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    static const UINT32 indices[] = {
        0, 1, 2,
        0, 1, 3,
        0, 2, 3,
        1, 2, 3,
    };

    FCL_MESH_GEOMETRY_DESC desc = {};
    desc.Vertices = vertices;
    desc.VertexCount = RTL_NUMBER_OF(vertices);
    desc.Indices = indices;
    desc.IndexCount = RTL_NUMBER_OF(indices);
    return FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, handle);
}

NTSTATUS EvaluateCollision(
    FCL_GEOMETRY_HANDLE sphereA,
    FCL_GEOMETRY_HANDLE sphereB,
    _Out_ PBOOLEAN collisionDetected,
    _Out_ PFCL_CONTACT_SUMMARY summary) noexcept {
    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation.X = kCollisionSeparation;

    BOOLEAN isColliding = FALSE;
    FCL_CONTACT_INFO contact = {};

    NTSTATUS status = FclCollisionDetect(
        sphereA,
        &transformA,
        sphereB,
        &transformB,
        &isColliding,
        &contact);

    if (NT_SUCCESS(status) && !isColliding) {
        status = STATUS_DATA_ERROR;
    }

    if (collisionDetected != nullptr) {
        *collisionDetected = NT_SUCCESS(status) ? TRUE : FALSE;
    }

    if (NT_SUCCESS(status)) {
        CopyContactSummary(summary, contact);
    } else if (summary != nullptr) {
        RtlZeroMemory(summary, sizeof(*summary));
    }

    return status;
}

NTSTATUS DestroyAllHandles(
    _Inout_ PFCL_GEOMETRY_HANDLE handleA,
    _Inout_ PFCL_GEOMETRY_HANDLE handleB) noexcept {
    NTSTATUS aggregate = STATUS_SUCCESS;
    const NTSTATUS statusA = DestroyHandle(handleA);
    const NTSTATUS statusB = DestroyHandle(handleB);
    if (!NT_SUCCESS(statusA)) {
        aggregate = statusA;
    }
    if (!NT_SUCCESS(statusB) && NT_SUCCESS(aggregate)) {
        aggregate = statusB;
    }
    return aggregate;
}

NTSTATUS RunInvalidGeometryScenario(_Out_ NTSTATUS* observedStatus) noexcept {
    if (observedStatus == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = 0.0f;
    FCL_GEOMETRY_HANDLE handle = {};
    const NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &handle);
    *observedStatus = status;
    if (status != STATUS_INVALID_PARAMETER) {
        return STATUS_DATA_ERROR;
    }
    return STATUS_SUCCESS;
}

NTSTATUS RunDestroyInvalidScenario(_Out_ NTSTATUS* observedStatus) noexcept {
    if (observedStatus == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    FCL_GEOMETRY_HANDLE temp = {};
    NTSTATUS status = CreateSphere(0.25f, &temp);
    if (!NT_SUCCESS(status)) {
        *observedStatus = status;
        return status;
    }
    status = FclDestroyGeometry(temp);
    if (!NT_SUCCESS(status)) {
        *observedStatus = status;
        return status;
    }
    status = FclDestroyGeometry(temp);
    *observedStatus = status;
    if (status != STATUS_INVALID_HANDLE) {
        return STATUS_DATA_ERROR;
    }
    return STATUS_SUCCESS;
}

NTSTATUS RunCollisionInvalidScenario(_Out_ NTSTATUS* observedStatus) noexcept {
    if (observedStatus == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    BOOLEAN dummy = FALSE;
    const NTSTATUS status = FclCollisionDetect(
        {},
        nullptr,
        {},
        nullptr,
        &dummy,
        nullptr);
    *observedStatus = status;
    if (status != STATUS_INVALID_HANDLE) {
        return STATUS_DATA_ERROR;
    }
    return STATUS_SUCCESS;
}

NTSTATUS RunMeshGjkScenario() noexcept {
    FCL_GEOMETRY_HANDLE meshA = {};
    FCL_GEOMETRY_HANDLE meshB = {};
    NTSTATUS status = CreateUnitMesh(&meshA);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = CreateUnitMesh(&meshB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(meshA);
        return status;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation = {3.0f, 0.0f, 0.0f};

    BOOLEAN isColliding = FALSE;
    status = FclCollisionDetect(meshA, &transformA, meshB, &transformB, &isColliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (isColliding) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

    transformB.Translation = {0.5f, 0.5f, 0.5f};
    status = FclCollisionDetect(meshA, &transformA, meshB, &transformB, &isColliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (!isColliding) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

Cleanup:
    FclDestroyGeometry(meshB);
    FclDestroyGeometry(meshA);
    return status;
}

NTSTATUS RunSphereMeshScenario() noexcept {
    FCL_GEOMETRY_HANDLE sphere = {};
    FCL_GEOMETRY_HANDLE mesh = {};
    NTSTATUS status = CreateSphere(0.5f, &sphere);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = CreateUnitMesh(&mesh);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphere);
        return status;
    }

    FCL_TRANSFORM sphereTransform = IdentityTransform();
    FCL_TRANSFORM meshTransform = IdentityTransform();
    BOOLEAN isColliding = FALSE;

    sphereTransform.Translation = {3.0f, 0.0f, 0.0f};
    status = FclCollisionDetect(sphere, &sphereTransform, mesh, &meshTransform, &isColliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (isColliding) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

    sphereTransform.Translation = {0.3f, 0.3f, 0.3f};
    status = FclCollisionDetect(sphere, &sphereTransform, mesh, &meshTransform, &isColliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (!isColliding) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

Cleanup:
    FclDestroyGeometry(mesh);
    FclDestroyGeometry(sphere);
    return status;
}

NTSTATUS RunContinuousCollisionScenario() noexcept {
    FCL_GEOMETRY_HANDLE sphere = {};
    FCL_GEOMETRY_HANDLE obb = {};

    FCL_SPHERE_GEOMETRY_DESC sphereDesc = {};
    sphereDesc.Center = {0.0f, 0.0f, 0.0f};
    sphereDesc.Radius = 0.5f;

    FCL_OBB_GEOMETRY_DESC obbDesc = {};
    obbDesc.Center = {0.0f, 0.0f, 0.0f};
    obbDesc.Extents = {0.5f, 0.5f, 0.5f};
    obbDesc.Rotation = IdentityTransform().Rotation;

    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphereDesc, &sphere);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = FclCreateGeometry(FCL_GEOMETRY_OBB, &obbDesc, &obb);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphere);
        return status;
    }

    FCL_INTERP_MOTION_DESC motionDesc = {};
    motionDesc.Start = IdentityTransform();
    motionDesc.End = IdentityTransform();
    motionDesc.End.Translation.X = 2.0f;

    FCL_INTERP_MOTION sphereMotion = {};
    status = FclInterpMotionInitialize(&motionDesc, &sphereMotion);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    motionDesc = {};
    motionDesc.Start = IdentityTransform();
    motionDesc.Start.Translation.X = 3.0f;
    motionDesc.End = IdentityTransform();
    FCL_INTERP_MOTION obbMotion = {};
    status = FclInterpMotionInitialize(&motionDesc, &obbMotion);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    FCL_CONTINUOUS_COLLISION_QUERY query = {};
    query.Object1 = sphere;
    query.Motion1 = sphereMotion;
    query.Object2 = obb;
    query.Motion2 = obbMotion;
    query.Tolerance = 1e-4;
    query.MaxIterations = 32;

    FCL_CONTINUOUS_COLLISION_RESULT result = {};
    status = FclContinuousCollision(&query, &result);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (!result.Intersecting || result.TimeOfImpact > 1.0) {
        status = STATUS_DATA_ERROR;
    }

Cleanup:
    FclDestroyGeometry(obb);
    FclDestroyGeometry(sphere);
    return status;
}

NTSTATUS RunGeometryUpdateScenario() noexcept {
    static const FCL_VECTOR3 vertices[] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    static const UINT32 indices[] = {
        0, 1, 2,
        0, 1, 3,
        0, 2, 3,
        1, 2, 3,
    };

    FCL_MESH_GEOMETRY_DESC desc = {};
    desc.Vertices = vertices;
    desc.VertexCount = RTL_NUMBER_OF(vertices);
    desc.Indices = indices;
    desc.IndexCount = RTL_NUMBER_OF(indices);

    FCL_GEOMETRY_HANDLE handle = {};
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, &handle);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    static const FCL_VECTOR3 verticesUpdated[] = {
        {0.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {0.0f, 2.0f, 0.0f},
        {0.0f, 0.0f, 2.0f},
    };
    static const UINT32 indicesUpdated[] = {
        0, 1, 2,
        0, 1, 3,
        0, 2, 3,
        1, 2, 3,
    };

    FCL_MESH_GEOMETRY_DESC updated = {};
    updated.Vertices = verticesUpdated;
    updated.VertexCount = RTL_NUMBER_OF(verticesUpdated);
    updated.Indices = indicesUpdated;
    updated.IndexCount = RTL_NUMBER_OF(indicesUpdated);

    status = FclUpdateMeshGeometry(handle, &updated);
    NTSTATUS cleanupStatus = FclDestroyGeometry(handle);
    if (NT_SUCCESS(status) && !NT_SUCCESS(cleanupStatus)) {
        status = cleanupStatus;
    }
    return status;
}

NTSTATUS RunSphereObbScenario() noexcept {
    FCL_GEOMETRY_HANDLE sphere = {};
    FCL_GEOMETRY_HANDLE obb = {};

    FCL_SPHERE_GEOMETRY_DESC sphereDesc = {};
    sphereDesc.Center = {0.0f, 0.0f, 0.0f};
    sphereDesc.Radius = 0.5f;

    FCL_OBB_GEOMETRY_DESC obbDesc = {};
    obbDesc.Center = {0.0f, 0.0f, 0.0f};
    obbDesc.Extents = {0.4f, 0.4f, 0.4f};
    obbDesc.Rotation = IdentityTransform().Rotation;

    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphereDesc, &sphere);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = FclCreateGeometry(FCL_GEOMETRY_OBB, &obbDesc, &obb);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphere);
        return status;
    }

    FCL_TRANSFORM sphereTransform = IdentityTransform();
    FCL_TRANSFORM obbTransform = IdentityTransform();
    obbTransform.Translation.X = 2.0f;

    BOOLEAN colliding = FALSE;
    status = FclCollisionDetect(sphere, &sphereTransform, obb, &obbTransform, &colliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (colliding) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

    obbTransform.Translation.X = 0.6f;
    FCL_CONTACT_INFO contact = {};
    status = FclCollisionDetect(sphere, &sphereTransform, obb, &obbTransform, &colliding, &contact);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (!colliding || contact.PenetrationDepth <= 0.0f) {
        status = STATUS_DATA_ERROR;
    }

Cleanup:
    FclDestroyGeometry(obb);
    FclDestroyGeometry(sphere);
    return status;
}

NTSTATUS RunMeshComplexScenario() noexcept {
    FCL_GEOMETRY_HANDLE meshA = {};
    FCL_GEOMETRY_HANDLE meshB = {};

    NTSTATUS status = CreateUnitMesh(&meshA);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = CreateUnitMesh(&meshB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(meshA);
        return status;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation = {3.0f, 0.0f, 0.0f};

    BOOLEAN colliding = FALSE;
    status = FclCollisionDetect(meshA, &transformA, meshB, &transformB, &colliding, nullptr);
    if (!NT_SUCCESS(status) || colliding) {
        goto Cleanup;
    }

    transformB.Translation = {0.2f, 0.2f, 0.2f};
    status = FclCollisionDetect(meshA, &transformA, meshB, &transformB, &colliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (!colliding) {
        status = STATUS_DATA_ERROR;
    }

Cleanup:
    FclDestroyGeometry(meshB);
    FclDestroyGeometry(meshA);
    return status;
}

NTSTATUS RunBoundaryScenario() noexcept {
    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    NTSTATUS status = CreateSphere(1.0f, &sphereA);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = CreateSphere(1.0f, &sphereB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphereA);
        return status;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation.X = 2.05f;

    BOOLEAN colliding = FALSE;
    status = FclCollisionDetect(sphereA, &transformA, sphereB, &transformB, &colliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (colliding) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

    transformB.Translation.X = 1.95f;
    FCL_CONTACT_INFO contact = {};
    status = FclCollisionDetect(sphereA, &transformA, sphereB, &transformB, &colliding, &contact);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (!colliding || contact.PenetrationDepth <= 0.0f) {
        status = STATUS_DATA_ERROR;
    }

Cleanup:
    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return status;
}

NTSTATUS RunDriverVerifierScenario(_Out_ PBOOLEAN isActive) noexcept {
    if (isActive == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
#if (NTDDI_VERSION >= NTDDI_WIN7)
    *isActive = MmIsDriverVerifierActive() ? TRUE : FALSE;
#else
    *isActive = FALSE;
#endif
    return STATUS_SUCCESS;
}

NTSTATUS RunLeakDetectionScenario() noexcept {
    const auto before = fclmusa::memory::QueryStats();

    FCL_GEOMETRY_HANDLE sphere = {};
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = 1.0f;
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &sphere);
    if (NT_SUCCESS(status)) {
        status = FclDestroyGeometry(sphere);
    }

    const auto after = fclmusa::memory::QueryStats();
    if (!NT_SUCCESS(status)) {
        return status;
    }
    return (after.BytesInUse == before.BytesInUse) ? STATUS_SUCCESS : STATUS_DATA_ERROR;
}

NTSTATUS RunStressScenario(ULONGLONG* durationMicroseconds) noexcept {
    if (durationMicroseconds == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *durationMicroseconds = 0;

    FCL_GEOMETRY_HANDLE a = {};
    FCL_GEOMETRY_HANDLE b = {};
    NTSTATUS status = CreateSphere(0.75f, &a);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = CreateSphere(0.75f, &b);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(a);
        return status;
    }

    FCL_TRANSFORM ta = IdentityTransform();
    FCL_TRANSFORM tb = IdentityTransform();

    ULONGLONG start = QueryTimeMicroseconds();
    for (ULONG iteration = 0; iteration < 256; ++iteration) {
        tb.Translation.X = static_cast<float>(iteration % 4) * 0.25f;
        BOOLEAN colliding = FALSE;
        status = FclCollisionDetect(a, &ta, b, &tb, &colliding, nullptr);
        if (!NT_SUCCESS(status)) {
            break;
        }
        FCL_DISTANCE_RESULT distance = {};
        status = FclDistanceCompute(a, &ta, b, &tb, &distance);
        if (!NT_SUCCESS(status)) {
            break;
        }
    }
    ULONGLONG end = QueryTimeMicroseconds();
    *durationMicroseconds = end - start;

    FclDestroyGeometry(b);
    FclDestroyGeometry(a);
    return status;
}

NTSTATUS RunPerformanceScenario(ULONGLONG* durationMicroseconds) noexcept {
    if (durationMicroseconds == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *durationMicroseconds = 0;

    FCL_GEOMETRY_HANDLE mesh = {};
    NTSTATUS status = CreateUnitMesh(&mesh);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    FCL_TRANSFORM t = IdentityTransform();
    t.Translation = {0.1f, -0.2f, 0.3f};

    ULONGLONG start = QueryTimeMicroseconds();
    for (ULONG iteration = 0; iteration < 128; ++iteration) {
        BOOLEAN colliding = FALSE;
        status = FclCollisionDetect(mesh, &IdentityTransform(), mesh, &t, &colliding, nullptr);
        if (!NT_SUCCESS(status)) {
            break;
        }
        t.Translation.X += 0.01f;
    }
    ULONGLONG end = QueryTimeMicroseconds();
    *durationMicroseconds = end - start;

    FclDestroyGeometry(mesh);
    return status;
}

NTSTATUS RunMeshBroadphaseScenario(_Out_opt_ PULONG intersectingPairs) noexcept {
    FCL_GEOMETRY_HANDLE meshA = {};
    FCL_GEOMETRY_HANDLE meshB = {};
    NTSTATUS status = CreateUnitMesh(&meshA);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = CreateUnitMesh(&meshB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(meshA);
        return status;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    FCL_BROADPHASE_OBJECT objects[2] = {
        {meshA, &transformA},
        {meshB, &transformB},
    };
    FCL_BROADPHASE_PAIR pairs[1] = {};
    ULONG pairCount = 0;

    transformB.Translation = {3.0f, 0.0f, 0.0f};
    status = FclBroadphaseDetect(objects, RTL_NUMBER_OF(objects), pairs, RTL_NUMBER_OF(pairs), &pairCount);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (pairCount != 0) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

    transformB.Translation = {0.3f, 0.3f, 0.3f};
    status = FclBroadphaseDetect(objects, RTL_NUMBER_OF(objects), pairs, RTL_NUMBER_OF(pairs), &pairCount);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (pairCount == 0) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

    if (intersectingPairs != nullptr) {
        *intersectingPairs = pairCount;
    }

Cleanup:
    FclDestroyGeometry(meshB);
    FclDestroyGeometry(meshA);
    return status;
}

}  // namespace

extern "C"
NTSTATUS
FclRunSelfTest(_Out_ PFCL_SELF_TEST_RESULT result) noexcept {
    if (result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(result, sizeof(*result));
    result->Version = *FclGetDriverVersion();
    result->PoolBefore = fclmusa::memory::QueryStats();

    NTSTATUS overall = STATUS_SUCCESS;

    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    FCL_DISTANCE_RESULT distanceResult = {};
    FCL_TRANSFORM distanceTransformA = IdentityTransform();
    FCL_TRANSFORM distanceTransformB = IdentityTransform();
    distanceTransformB.Translation.X = kCollisionSeparation;

    NTSTATUS status = FclInitialize();
    if (status == STATUS_ALREADY_INITIALIZED) {
        status = STATUS_SUCCESS;
    }
    result->InitializeStatus = status;
    if (!NT_SUCCESS(status)) {
        overall = status;
        goto Finalize;
    }

    status = CreateSphere(1.0f, &sphereA);
    if (NT_SUCCESS(status)) {
        status = CreateSphere(0.75f, &sphereB);
        if (!NT_SUCCESS(status)) {
            DestroyHandle(&sphereA);
        }
    }
    result->GeometryCreateStatus = status;
    if (!NT_SUCCESS(status)) {
        if (NT_SUCCESS(overall)) {
            overall = status;
        }
        goto DestroyStage;
    }

    status = EvaluateCollision(sphereA, sphereB, &result->CollisionDetected, &result->Contact);
    result->CollisionStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    status = FclDistanceCompute(
        sphereA,
        &distanceTransformA,
        sphereB,
        &distanceTransformB,
        &distanceResult);
    result->DistanceStatus = status;
    if (NT_SUCCESS(status)) {
        result->DistanceValue = distanceResult.Distance;
    } else if (NT_SUCCESS(overall)) {
        overall = status;
    }

    FCL_BROADPHASE_OBJECT bpObjects[2] = {
        {sphereA, &distanceTransformA},
        {sphereB, &distanceTransformB}};
    FCL_BROADPHASE_PAIR bpPairs[1] = {};
    ULONG bpPairCount = 0;
    status = FclBroadphaseDetect(bpObjects, RTL_NUMBER_OF(bpObjects), bpPairs, RTL_NUMBER_OF(bpPairs), &bpPairCount);
    result->BroadphaseStatus = status;
    result->BroadphasePairCount = bpPairCount;
    if (NT_SUCCESS(status)) {
        if (bpPairCount == 0) {
            status = STATUS_DATA_ERROR;
        }
    }
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    status = RunMeshGjkScenario();
    result->MeshGjkStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    const NTSTATUS sphereMeshStatus = RunSphereMeshScenario();
    result->SphereMeshStatus = sphereMeshStatus;
    if (!NT_SUCCESS(sphereMeshStatus) && NT_SUCCESS(overall)) {
        overall = sphereMeshStatus;
    }

    ULONG meshPairCount = 0;
    status = RunMeshBroadphaseScenario(&meshPairCount);
    result->MeshBroadphaseStatus = status;
    result->MeshBroadphasePairCount = meshPairCount;
    if (!NT_SUCCESS(status)) {
        if (NT_SUCCESS(overall)) {
            overall = status;
        }
        if (NT_SUCCESS(result->BroadphaseStatus)) {
            result->BroadphaseStatus = status;
        }
    }

    status = RunContinuousCollisionScenario();
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }
    result->ContinuousCollisionStatus = status;

    status = RunGeometryUpdateScenario();
    result->GeometryUpdateStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    status = RunSphereObbScenario();
    result->SphereObbStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    status = RunMeshComplexScenario();
    result->MeshComplexStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    status = RunBoundaryScenario();
    result->BoundaryStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    BOOLEAN verifierActive = FALSE;
    status = RunDriverVerifierScenario(&verifierActive);
    result->DriverVerifierStatus = status;
    result->DriverVerifierActive = verifierActive;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    status = RunLeakDetectionScenario();
    result->LeakTestStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    ULONGLONG stressDuration = 0;
    status = RunStressScenario(&stressDuration);
    result->StressStatus = status;
    result->StressDurationMicroseconds = stressDuration;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    ULONGLONG perfDuration = 0;
    status = RunPerformanceScenario(&perfDuration);
    result->PerformanceStatus = status;
    result->PerformanceDurationMicroseconds = perfDuration;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

DestroyStage:
    status = DestroyAllHandles(&sphereA, &sphereB);
    result->DestroyStatus = status;
    if (!NT_SUCCESS(status) && NT_SUCCESS(overall)) {
        overall = status;
    }

    BOOLEAN boundaryPassed = TRUE;
    status = RunInvalidGeometryScenario(&result->InvalidGeometryStatus);
    if (!NT_SUCCESS(status)) {
        boundaryPassed = FALSE;
        if (NT_SUCCESS(overall)) {
            overall = status;
        }
    }

    status = RunDestroyInvalidScenario(&result->DestroyInvalidStatus);
    if (!NT_SUCCESS(status)) {
        boundaryPassed = FALSE;
        if (NT_SUCCESS(overall)) {
            overall = status;
        }
    }

    status = RunCollisionInvalidScenario(&result->CollisionInvalidStatus);
    if (!NT_SUCCESS(status)) {
        boundaryPassed = FALSE;
        if (NT_SUCCESS(overall)) {
            overall = status;
        }
    }

    result->BoundaryPassed = boundaryPassed ? TRUE : FALSE;

Finalize:
    result->PoolAfter = fclmusa::memory::QueryStats();
    result->PoolBytesDelta = AbsoluteDifference(result->PoolAfter.BytesInUse, result->PoolBefore.BytesInUse);
    result->PoolBalanced = (result->PoolBytesDelta == 0);
    result->OverallStatus = overall;
    if (NT_SUCCESS(result->OverallStatus) && (!result->PoolBalanced || !result->BoundaryPassed)) {
        result->OverallStatus = STATUS_DATA_ERROR;
    }
    result->Passed = NT_SUCCESS(result->OverallStatus);
    return result->OverallStatus;
}

