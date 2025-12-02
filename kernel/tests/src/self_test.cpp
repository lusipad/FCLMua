#include <ntddk.h>
#include <wdm.h>

#include <cmath>

#include "fclmusa/broadphase.h"
#include "fclmusa/collision.h"
#include "fclmusa/distance.h"
#include "fclmusa/driver.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/geometry/geometry_tests.h"
#include "fclmusa/broadphase/broadphase_tests.h"
#include "fclmusa/upstream/upstream_tests.h"
#include "fclmusa/driver/ioctl_tests.h"
#include "fclmusa/collision/collision_core_tests.h"
#include "fclmusa/collision/ccd_core_tests.h"
#include "fclmusa/distance/distance_core_tests.h"
#include "fclmusa/logging.h"
#include "fclmusa/memory/pool_allocator.h"
#include "fclmusa/memory/pool_test.h"
#include "fclmusa/runtime/musa_runtime_adapter.h"
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

NTSTATUS RunMeshCollisionScenario() noexcept {
    FCL_GEOMETRY_HANDLE meshA = {};
    FCL_GEOMETRY_HANDLE meshB = {};

    NTSTATUS status = CreateUnitMesh(&meshA);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = CreateUnitMesh(&meshB);
    if (!NT_SUCCESS(status)) {
        DestroyHandle(&meshA);
        return status;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();

    // First place meshes far apart: expect no collision.
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

    // Then place meshes overlapping: expect collision.
    // NOTE: translation must satisfy d <= 1/3 so that two unit
    // tetrahedra around origin have non-empty intersection.
    transformB.Translation = {0.25f, 0.25f, 0.25f};
    isColliding = FALSE;
    status = FclCollisionDetect(meshA, &transformA, meshB, &transformB, &isColliding, nullptr);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    if (!isColliding) {
        status = STATUS_DATA_ERROR;
        goto Cleanup;
    }

Cleanup:
    DestroyHandle(&meshB);
    DestroyHandle(&meshA);
    return status;
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

NTSTATUS RunSphereCollisionScenario(
    _Out_ BOOLEAN* collisionDetected,
    _Out_ PFCL_CONTACT_SUMMARY summary,
    _Out_ ULONGLONG* elapsedMicroseconds) noexcept {
    if (collisionDetected == nullptr || summary == nullptr || elapsedMicroseconds == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};

    NTSTATUS status = CreateSphere(1.0f, &sphereA);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = CreateSphere(1.0f, &sphereB);
    if (!NT_SUCCESS(status)) {
        DestroyHandle(&sphereA);
        return status;
    }

    const ULONGLONG start = QueryTimeMicroseconds();
    status = EvaluateCollision(sphereA, sphereB, collisionDetected, summary);
    const ULONGLONG end = QueryTimeMicroseconds();

    *elapsedMicroseconds = AbsoluteDifference(end, start);

    const NTSTATUS destroyStatus = DestroyAllHandles(&sphereA, &sphereB);
    if (NT_SUCCESS(status) && !NT_SUCCESS(destroyStatus)) {
        status = destroyStatus;
    }

    return status;
}

NTSTATUS RunBroadphaseScenario(_Out_ PULONG broadphasePairCount) noexcept {
    if (broadphasePairCount == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *broadphasePairCount = 0;

    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    FCL_GEOMETRY_HANDLE sphereC = {};

    NTSTATUS status = CreateSphere(0.5f, &sphereA);
    if (!NT_SUCCESS(status)) {
    Cleanup:
        DestroyHandle(&sphereA);
        DestroyHandle(&sphereB);
        DestroyHandle(&sphereC);
        return status;
    }
    status = CreateSphere(0.5f, &sphereB);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }
    status = CreateSphere(0.5f, &sphereC);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    FCL_TRANSFORM transformC = IdentityTransform();

    transformA.Translation = {0.0f, 0.0f, 0.0f};
    transformB.Translation = {0.4f, 0.0f, 0.0f};
    transformC.Translation = {5.0f, 0.0f, 0.0f};

    FCL_BROADPHASE_OBJECT objects[3] = {};
    objects[0].Handle = sphereA;
    objects[0].Transform = &transformA;
    objects[1].Handle = sphereB;
    objects[1].Transform = &transformB;
    objects[2].Handle = sphereC;
    objects[2].Transform = &transformC;

    FCL_BROADPHASE_PAIR pairs[4] = {};
    ULONG pairCount = 0;
    status = FclBroadphaseDetect(objects, RTL_NUMBER_OF(objects), pairs, RTL_NUMBER_OF(pairs), &pairCount);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    *broadphasePairCount = pairCount;

    goto Cleanup;
}

NTSTATUS RunCcdScenario() noexcept {
    FCL_GEOMETRY_HANDLE moving = {};
    FCL_GEOMETRY_HANDLE target = {};

    NTSTATUS status = CreateSphere(0.5f, &moving);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = CreateSphere(0.5f, &target);
    if (!NT_SUCCESS(status)) {
        DestroyHandle(&moving);
        return status;
    }

    FCL_INTERP_MOTION_DESC motionDesc = {};
    motionDesc.Start = IdentityTransform();
    motionDesc.End = IdentityTransform();
    motionDesc.End.Translation.X = 2.0f;

    FCL_INTERP_MOTION motion = {};
    status = FclInterpMotionInitialize(&motionDesc, &motion);
    if (!NT_SUCCESS(status)) {
        DestroyHandle(&target);
        DestroyHandle(&moving);
        return status;
    }

    FCL_CONTINUOUS_COLLISION_QUERY query = {};
    query.Object1 = moving;
    query.Motion1 = motion;
    query.Object2 = target;
    query.Motion2 = motion;  // reuse for simplicity
    query.Tolerance = 1.0e-4;
    query.MaxIterations = 64;

    FCL_CONTINUOUS_COLLISION_RESULT result = {};
    status = FclContinuousCollision(&query, &result);

    DestroyHandle(&target);
    DestroyHandle(&moving);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (!result.Intersecting) {
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunSelfTestScenario(
    _In_ FCL_SELF_TEST_SCENARIO_ID scenarioId,
    _Out_ PFCL_SELF_TEST_SCENARIO_RESULT result) noexcept {
    if (result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(result, sizeof(*result));
    result->ScenarioId = scenarioId;

    const auto poolBefore = fclmusa::memory::QueryStats();

    NTSTATUS scenarioStatus = STATUS_INVALID_PARAMETER;

    switch (scenarioId) {
        case FCL_SELF_TEST_SCENARIO_RUNTIME: {
            scenarioStatus = FclRunMusaRuntimeSmokeTests();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_MEMORY: {
            scenarioStatus = FclRunMemoryPoolTests();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_GEOMETRY: {
            scenarioStatus = FclRunGeometryManagerTests();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_COLLISION_CORE: {
            scenarioStatus = FclRunCollisionCoreTests();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_DISTANCE: {
            scenarioStatus = FclRunDistanceCoreTests();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_UPSTREAM: {
            scenarioStatus = FclRunUpstreamBridgeTests();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_DRIVER: {
            scenarioStatus = FclRunDriverIoctlTests();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_SPHERE_COLLISION: {
            BOOLEAN collisionDetected = FALSE;
            FCL_CONTACT_SUMMARY summary = {};
            ULONGLONG elapsedMicroseconds = 0;
            scenarioStatus = RunSphereCollisionScenario(&collisionDetected, &summary, &elapsedMicroseconds);
            if (NT_SUCCESS(scenarioStatus)) {
                result->Contact = summary;
            }
            break;
        }
        case FCL_SELF_TEST_SCENARIO_BROADPHASE: {
            scenarioStatus = FclRunBroadphaseCoreTests();
            if (NT_SUCCESS(scenarioStatus)) {
                ULONG broadphasePairCount = 0;
                scenarioStatus = RunBroadphaseScenario(&broadphasePairCount);
            }
            break;
        }
        case FCL_SELF_TEST_SCENARIO_MESH_COLLISION: {
            scenarioStatus = RunMeshCollisionScenario();
            break;
        }
        case FCL_SELF_TEST_SCENARIO_CCD: {
            scenarioStatus = FclRunCcdCoreTests();
            if (NT_SUCCESS(scenarioStatus)) {
                scenarioStatus = RunCcdScenario();
            }
            break;
        }
        default:
            return STATUS_INVALID_PARAMETER;
    }

    const auto poolAfter = fclmusa::memory::QueryStats();

    result->Status = scenarioStatus;
    result->PoolBefore = poolBefore;
    result->PoolAfter = poolAfter;

    // �����ʾ��ϸ���룬������ result->Step/Reserved ����չ��

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclRunSelfTest(
    _Out_ PFCL_SELF_TEST_RESULT result) noexcept {
    if (result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(result, sizeof(*result));

    const FCL_DRIVER_VERSION* version = FclGetDriverVersion();
    if (version != nullptr) {
        result->Version = *version;
    }

    const auto poolBefore = fclmusa::memory::QueryStats();

    auto accumulateOverall = [](NTSTATUS currentOverall, NTSTATUS next) noexcept {
        if (!NT_SUCCESS(next) && NT_SUCCESS(currentOverall)) {
            return next;
        }
        return currentOverall;
    };

    NTSTATUS overallStatus = STATUS_SUCCESS;

    // 1. Musa.Runtime STL smoke
    result->InitializeStatus = FclRunMusaRuntimeSmokeTests();
    overallStatus = accumulateOverall(overallStatus, result->InitializeStatus);

    // 1.5 Pool allocator / leak checks
    result->LeakTestStatus = FclRunMemoryPoolTests();
    overallStatus = accumulateOverall(overallStatus, result->LeakTestStatus);

    // 1.75 Geometry manager
    result->GeometryCreateStatus = FclRunGeometryManagerTests();
    result->GeometryUpdateStatus = result->GeometryCreateStatus;
    overallStatus = accumulateOverall(overallStatus, result->GeometryCreateStatus);

    // 1.8 Collision input validation
    result->SphereObbStatus = FclRunCollisionCoreTests();
    overallStatus = accumulateOverall(overallStatus, result->SphereObbStatus);

    // 1.85 Distance validation
    result->DistanceStatus = FclRunDistanceCoreTests();
    overallStatus = accumulateOverall(overallStatus, result->DistanceStatus);

    // 1.9 Upstream bridge smoke
    result->MeshBroadphaseStatus = FclRunUpstreamBridgeTests();
    overallStatus = accumulateOverall(overallStatus, result->MeshBroadphaseStatus);

    // 1.95 Driver IOCTL smoke
    result->SphereMeshStatus = FclRunDriverIoctlTests();
    overallStatus = accumulateOverall(overallStatus, result->SphereMeshStatus);

    // 2. ��ײ������
    BOOLEAN collisionDetected = FALSE;
    FCL_CONTACT_SUMMARY summary = {};
    ULONGLONG elapsedMicroseconds = 0;
    result->CollisionStatus = RunSphereCollisionScenario(&collisionDetected, &summary, &elapsedMicroseconds);
    if (NT_SUCCESS(result->CollisionStatus)) {
        result->CollisionDetected = collisionDetected ? TRUE : FALSE;
        result->Contact = summary;
    }
    overallStatus = accumulateOverall(overallStatus, result->CollisionStatus);

    // 3. Broadphase �۲�
    ULONG broadphasePairCount = 0;
    result->BroadphaseStatus = FclRunBroadphaseCoreTests();
    if (NT_SUCCESS(result->BroadphaseStatus)) {
        result->BroadphaseStatus = RunBroadphaseScenario(&broadphasePairCount);
        if (NT_SUCCESS(result->BroadphaseStatus)) {
            result->BroadphasePairCount = broadphasePairCount;
        }
    }
    overallStatus = accumulateOverall(overallStatus, result->BroadphaseStatus);

    // 4. Mesh GJK ����
    result->MeshGjkStatus = RunMeshCollisionScenario();
    overallStatus = accumulateOverall(overallStatus, result->MeshGjkStatus);

    // 5. CCD ����
    result->ContinuousCollisionStatus = FclRunCcdCoreTests();
    if (NT_SUCCESS(result->ContinuousCollisionStatus)) {
        result->ContinuousCollisionStatus = RunCcdScenario();
    }
    overallStatus = accumulateOverall(overallStatus, result->ContinuousCollisionStatus);

    const auto poolAfter = fclmusa::memory::QueryStats();
    result->PoolBefore = poolBefore;
    result->PoolAfter = poolAfter;
    result->PoolBytesDelta = AbsoluteDifference(poolBefore.BytesInUse, poolAfter.BytesInUse);
    result->PoolBalanced = (poolBefore.BytesInUse == poolAfter.BytesInUse) ? TRUE : FALSE;

    result->OverallStatus = overallStatus;
    result->Passed = NT_SUCCESS(overallStatus) ? TRUE : FALSE;

    return STATUS_SUCCESS;
}
