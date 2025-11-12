#include <ntddk.h>
#include <ntintsafe.h>
#include <wdm.h>

#include <algorithm>
#include <cmath>
#include <float.h>
#include <utility>

#include "fclmusa/distance.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/geometry/obb.h"
#include "fclmusa/logging.h"

namespace {

using std::max;
using std::swap;

using namespace fclmusa::geom;
using std::max;
using std::swap;

constexpr float kDistanceTolerance = kLinearTolerance;

void InitializeResult(_Out_ PFCL_DISTANCE_RESULT result) noexcept {
    RtlZeroMemory(result, sizeof(*result));
    result->Distance = 0.0f;
}

NTSTATUS DistanceSphereSphere(
    const FCL_GEOMETRY_SNAPSHOT& sphereA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& sphereB,
    const FCL_TRANSFORM& transformB,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept {
    const auto& descA = sphereA.Data.Sphere;
    const auto& descB = sphereB.Data.Sphere;
    if (descA.Radius <= 0.0f || descB.Radius <= 0.0f) {
        return STATUS_INVALID_PARAMETER;
    }

    const FCL_VECTOR3 centerA = TransformPoint(transformA, descA.Center);
    const FCL_VECTOR3 centerB = TransformPoint(transformB, descB.Center);
    const FCL_VECTOR3 delta = Subtract(centerB, centerA);
    const float centerDistance = Length(delta);
    const float target = descA.Radius + descB.Radius;
    float distance = centerDistance - target;
    if (distance < 0.0f) {
        distance = 0.0f;
    }

    const FCL_VECTOR3 normal = Normalize(centerDistance <= kSingularityEpsilon ? FCL_VECTOR3{1.0f, 0.0f, 0.0f} : delta);
    result->Distance = distance;
    result->ClosestPoint1 = Add(centerA, Scale(normal, descA.Radius));
    result->ClosestPoint2 = Subtract(centerB, Scale(normal, descB.Radius));
    return STATUS_SUCCESS;
}

NTSTATUS DistanceSphereObb(
    const FCL_GEOMETRY_SNAPSHOT& sphere,
    const FCL_TRANSFORM& sphereTransform,
    const FCL_GEOMETRY_SNAPSHOT& obb,
    const FCL_TRANSFORM& obbTransform,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept {
    const auto& descSphere = sphere.Data.Sphere;
    if (descSphere.Radius <= 0.0f) {
        return STATUS_INVALID_PARAMETER;
    }

    const OrientedBox box = BuildWorldObb(obb.Data.Obb, obbTransform);
    const FCL_VECTOR3 sphereCenter = TransformPoint(sphereTransform, descSphere.Center);
    const FCL_VECTOR3 closest = ClosestPointOnObb(box, sphereCenter);
    const FCL_VECTOR3 delta = Subtract(closest, sphereCenter);
    const float len = Length(delta);
    const float distance = max(len - descSphere.Radius, 0.0f);

    FCL_VECTOR3 normal = {1.0f, 0.0f, 0.0f};
    if (len > kSingularityEpsilon) {
        normal = Scale(delta, 1.0f / len);
    }

    result->Distance = distance;
    result->ClosestPoint1 = Add(sphereCenter, Scale(normal, descSphere.Radius));
    result->ClosestPoint2 = closest;
    return STATUS_SUCCESS;
}

NTSTATUS DistanceObbObb(
    const FCL_GEOMETRY_SNAPSHOT& obbA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& obbB,
    const FCL_TRANSFORM& transformB,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept {
    const OrientedBox boxA = BuildWorldObb(obbA.Data.Obb, transformA);
    const OrientedBox boxB = BuildWorldObb(obbB.Data.Obb, transformB);

    float R[3][3];
    float AbsR[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = Dot(boxA.Axes[i], boxB.Axes[j]);
            AbsR[i][j] = fabs(R[i][j]) + kAxisEpsilon;
        }
    }

    const FCL_VECTOR3 translationWorld = Subtract(boxB.Center, boxA.Center);
    const float t[3] = {
        Dot(translationWorld, boxA.Axes[0]),
        Dot(translationWorld, boxA.Axes[1]),
        Dot(translationWorld, boxA.Axes[2])};

    bool separated = false;
    float bestGap = FLT_MAX;
    FCL_VECTOR3 bestAxis = {1.0f, 0.0f, 0.0f};

    auto recordAxis = [&](const FCL_VECTOR3& axisWorld, float projection, float radiusSum) {
        if (projection <= radiusSum + kDistanceTolerance) {
            return;
        }
        separated = true;
        const float gap = projection - radiusSum;
        FCL_VECTOR3 axis = Normalize(axisWorld);
        if (Length(axis) <= kSingularityEpsilon) {
            return;
        }
        if (Dot(axis, translationWorld) < 0.0f) {
            axis = Scale(axis, -1.0f);
        }
        if (gap < bestGap) {
            bestGap = gap;
            bestAxis = axis;
        }
    };

    for (int i = 0; i < 3; ++i) {
        const float ra = (&boxA.Extents.X)[i];
        const float rb = (&boxB.Extents.X)[0] * AbsR[i][0] +
                         (&boxB.Extents.X)[1] * AbsR[i][1] +
                         (&boxB.Extents.X)[2] * AbsR[i][2];
        recordAxis(boxA.Axes[i], fabs(t[i]), ra + rb);
    }

    for (int j = 0; j < 3; ++j) {
        const float rb = (&boxB.Extents.X)[j];
        const float ra = (&boxA.Extents.X)[0] * AbsR[0][j] +
                         (&boxA.Extents.X)[1] * AbsR[1][j] +
                         (&boxA.Extents.X)[2] * AbsR[2][j];
        recordAxis(boxB.Axes[j], fabs(Dot(translationWorld, boxB.Axes[j])), ra + rb);
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            FCL_VECTOR3 axis = Cross(boxA.Axes[i], boxB.Axes[j]);
            if (Length(axis) <= kAxisEpsilon) {
                continue;
            }

            const float ra = (&boxA.Extents.X)[(i + 1) % 3] * AbsR[(i + 2) % 3][j] +
                             (&boxA.Extents.X)[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
            const float rb = (&boxB.Extents.X)[(j + 1) % 3] * AbsR[i][(j + 2) % 3] +
                             (&boxB.Extents.X)[(j + 2) % 3] * AbsR[i][(j + 1) % 3];
            const float proj = fabs(t[(i + 1) % 3] * R[(i + 2) % 3][j] - t[(i + 2) % 3] * R[(i + 1) % 3][j]);
            recordAxis(axis, proj, ra + rb);
        }
    }

    if (!separated) {
        result->Distance = 0.0f;
        result->ClosestPoint1 = boxA.Center;
        result->ClosestPoint2 = boxB.Center;
        return STATUS_SUCCESS;
    }

    result->Distance = bestGap;
    result->ClosestPoint1 = SupportPoint(boxA, bestAxis);
    result->ClosestPoint2 = SupportPoint(boxB, Scale(bestAxis, -1.0f));
    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclDistanceCompute(
    _In_ FCL_GEOMETRY_HANDLE object1,
    _In_opt_ const FCL_TRANSFORM* transform1,
    _In_ FCL_GEOMETRY_HANDLE object2,
    _In_opt_ const FCL_TRANSFORM* transform2,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept {
    if (result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    InitializeResult(result);

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    FCL_TRANSFORM resolvedTransform1 = {};
    FCL_TRANSFORM resolvedTransform2 = {};
    if (transform1 != nullptr) {
        resolvedTransform1 = *transform1;
        if (!IsValidTransform(resolvedTransform1)) {
            return STATUS_INVALID_PARAMETER;
        }
    } else {
        resolvedTransform1 = IdentityTransform();
    }

    if (transform2 != nullptr) {
        resolvedTransform2 = *transform2;
        if (!IsValidTransform(resolvedTransform2)) {
            return STATUS_INVALID_PARAMETER;
        }
    } else {
        resolvedTransform2 = IdentityTransform();
    }

    FCL_GEOMETRY_REFERENCE reference1 = {};
    FCL_GEOMETRY_REFERENCE reference2 = {};
    FCL_GEOMETRY_SNAPSHOT snapshot1 = {};
    FCL_GEOMETRY_SNAPSHOT snapshot2 = {};

    NTSTATUS status = FclAcquireGeometryReference(object1, &reference1, &snapshot1);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FclAcquireGeometryReference(object2, &reference2, &snapshot2);
    if (!NT_SUCCESS(status)) {
        FclReleaseGeometryReference(&reference1);
        return status;
    }

    switch (snapshot1.Type) {
        case FCL_GEOMETRY_SPHERE:
            if (snapshot2.Type == FCL_GEOMETRY_SPHERE) {
                status = DistanceSphereSphere(snapshot1, resolvedTransform1, snapshot2, resolvedTransform2, result);
            } else if (snapshot2.Type == FCL_GEOMETRY_OBB) {
                status = DistanceSphereObb(snapshot1, resolvedTransform1, snapshot2, resolvedTransform2, result);
            } else {
                status = STATUS_NOT_IMPLEMENTED;
            }
            break;
        case FCL_GEOMETRY_OBB:
            if (snapshot2.Type == FCL_GEOMETRY_SPHERE) {
                status = DistanceSphereObb(snapshot2, resolvedTransform2, snapshot1, resolvedTransform1, result);
                if (NT_SUCCESS(status)) {
                    std::swap(result->ClosestPoint1, result->ClosestPoint2);
                }
            } else if (snapshot2.Type == FCL_GEOMETRY_OBB) {
                status = DistanceObbObb(snapshot1, resolvedTransform1, snapshot2, resolvedTransform2, result);
            } else {
                status = STATUS_NOT_IMPLEMENTED;
            }
            break;
        default:
            status = STATUS_NOT_IMPLEMENTED;
            break;
    }

    FclReleaseGeometryReference(&reference2);
    FclReleaseGeometryReference(&reference1);
    return status;
}


