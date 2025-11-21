#include <ntddk.h>
#include <wdm.h>

#include "fclmusa/collision.h"
#include "fclmusa/driver.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/upstream/upstream_bridge.h"

namespace {

using namespace fclmusa::geom;

constexpr double kDefaultTolerance = 1e-4;
constexpr ULONG kDefaultIterations = 64;

double Clamp01(double value) noexcept {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

struct MotionObject {
    FCL_GEOMETRY_REFERENCE Reference = {};
    FCL_GEOMETRY_SNAPSHOT Snapshot = {};

    ~MotionObject() {
        FclReleaseGeometryReference(&Reference);
    }
};

NTSTATUS InitializeMotionObject(
    FCL_GEOMETRY_HANDLE handle,
    _Out_ MotionObject* object) noexcept {
    if (object == nullptr || !FclIsGeometryHandleValid(handle)) {
        return STATUS_INVALID_HANDLE;
    }

    object->Reference = {};
    object->Snapshot = {};
    return FclAcquireGeometryReference(handle, &object->Reference, &object->Snapshot);
}

ULONGLONG QueryTimeMicroseconds() noexcept {
    LARGE_INTEGER frequency = {};
    const LARGE_INTEGER counter = KeQueryPerformanceCounter(&frequency);
    if (frequency.QuadPart == 0) {
        return 0;
    }
    const LONGLONG ticks = counter.QuadPart;
    const ULONGLONG absoluteTicks = (ticks >= 0)
        ? static_cast<ULONGLONG>(ticks)
        : static_cast<ULONGLONG>(-ticks);
    return (absoluteTicks * 1'000'000ULL) / static_cast<ULONGLONG>(frequency.QuadPart);
}

ULONGLONG AbsoluteDifference(ULONGLONG a, ULONGLONG b) noexcept {
    return (a > b) ? (a - b) : (b - a);
}

}  // namespace

extern "C"
NTSTATUS
FclContinuousCollisionCoreFromSnapshots(
    _In_ const FCL_GEOMETRY_SNAPSHOT* object1,
    _In_ const FCL_INTERP_MOTION* motion1,
    _In_ const FCL_GEOMETRY_SNAPSHOT* object2,
    _In_ const FCL_INTERP_MOTION* motion2,
    _In_ double tolerance,
    _In_ ULONG maxIterations,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept {
    if (object1 == nullptr || object2 == nullptr || motion1 == nullptr || motion2 == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    const double resolvedTolerance = (tolerance > 0.0) ? tolerance : kDefaultTolerance;
    const ULONG resolvedIterations = (maxIterations > 0) ? maxIterations : kDefaultIterations;

    const ULONGLONG start = QueryTimeMicroseconds();
    NTSTATUS status = FclUpstreamContinuousCollision(
        *object1,
        *motion1,
        *object2,
        *motion2,
        resolvedTolerance,
        resolvedIterations,
        result);
    const ULONGLONG end = QueryTimeMicroseconds();

    if (NT_SUCCESS(status) && start != 0 && end != 0) {
        const ULONGLONG elapsed = AbsoluteDifference(end, start);
        if (elapsed != 0) {
            FclDiagnosticsRecordContinuousCollisionDuration(elapsed);
        }
    }

    return status;
}

extern "C"
NTSTATUS
FclInterpMotionInitialize(
    _In_ const FCL_INTERP_MOTION_DESC* desc,
    _Out_ PFCL_INTERP_MOTION motion) noexcept {
    if (desc == nullptr || motion == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    motion->Start = desc->Start;
    motion->End = desc->End;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclInterpMotionEvaluate(
    _In_ const FCL_INTERP_MOTION* motion,
    _In_ double t,
    _Out_ PFCL_TRANSFORM transform) noexcept {
    if (motion == nullptr || transform == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    const double clamped = Clamp01(t);
    const FCL_VECTOR3 translation = LerpVector(
        motion->Start.Translation,
        motion->End.Translation,
        clamped);

    const FCL_QUATERNION startQuat = QuaternionFromMatrix(motion->Start.Rotation);
    const FCL_QUATERNION endQuat = QuaternionFromMatrix(motion->End.Rotation);

    FCL_QUATERNION lerp = {};
    lerp.W = static_cast<float>((1.0 - clamped) * startQuat.W + clamped * endQuat.W);
    lerp.X = static_cast<float>((1.0 - clamped) * startQuat.X + clamped * endQuat.X);
    lerp.Y = static_cast<float>((1.0 - clamped) * startQuat.Y + clamped * endQuat.Y);
    lerp.Z = static_cast<float>((1.0 - clamped) * startQuat.Z + clamped * endQuat.Z);

    transform->Rotation = QuaternionToMatrix(lerp);
    transform->Translation = translation;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclContinuousCollision(
    _In_ const FCL_CONTINUOUS_COLLISION_QUERY* query,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept {
    if (query == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    MotionObject objectA;
    NTSTATUS status = InitializeMotionObject(query->Object1, &objectA);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    MotionObject objectB;
    status = InitializeMotionObject(query->Object2, &objectB);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return FclContinuousCollisionCoreFromSnapshots(
        &objectA.Snapshot,
        &query->Motion1,
        &objectB.Snapshot,
        &query->Motion2,
        query->Tolerance,
        query->MaxIterations,
        result);
}
