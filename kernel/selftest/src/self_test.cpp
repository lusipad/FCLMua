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

}  // namespace

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

    NTSTATUS status = FclRunMusaRuntimeSmokeTests();
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Self-test: Musa.Runtime smoke failed: 0x%X", status);
        return status;
    }

    BOOLEAN collisionDetected = FALSE;
    FCL_CONTACT_SUMMARY summary = {};
    ULONGLONG elapsedMicroseconds = 0;
    ULONG broadphasePairCount = 0;

    status = RunSphereCollisionScenario(&collisionDetected, &summary, &elapsedMicroseconds);
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Self-test: collision scenario failed: 0x%X", status);
        return status;
    }

    status = RunBroadphaseScenario(&broadphasePairCount);
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Self-test: broadphase scenario failed: 0x%X", status);
        return status;
    }

    result->CollisionDetected = collisionDetected ? TRUE : FALSE;
    result->Contact = summary;
    result->BroadphasePairCount = broadphasePairCount;
    result->DistanceValue = 0.0f;
    result->OverallStatus = STATUS_SUCCESS;

    return STATUS_SUCCESS;
}
