#include <ntddk.h>
#include <ntintsafe.h>
#include <wdm.h>

#include <cmath>
#include <float.h>
#include <utility>

#include "fclmusa/collision.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/geometry/obb.h"
#include "fclmusa/logging.h"
#include "fclmusa/narrowphase/gjk.h"

namespace {

using namespace fclmusa::geom;
using std::swap;

constexpr float kCollisionTolerance = kLinearTolerance;
constexpr float kSingularityThreshold = kSingularityEpsilon;
constexpr float kAxisThreshold = kAxisEpsilon;
constexpr size_t kGeometryTypeCount = static_cast<size_t>(FCL_GEOMETRY_MESH) + 1;

using CollisionDispatchHandler = NTSTATUS (*)(
    const FCL_GEOMETRY_SNAPSHOT& objectA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& objectB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

struct CollisionObject {
    FCL_GEOMETRY_REFERENCE Reference = {};
    FCL_GEOMETRY_SNAPSHOT Snapshot = {};
    FCL_TRANSFORM Transform = {};

    ~CollisionObject() {
        FclReleaseGeometryReference(&Reference);
    }
};

NTSTATUS DispatchSphereSphere(
    const FCL_GEOMETRY_SNAPSHOT& objectA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& objectB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

NTSTATUS DispatchSphereObb(
    const FCL_GEOMETRY_SNAPSHOT& sphere,
    const FCL_TRANSFORM& sphereTransform,
    const FCL_GEOMETRY_SNAPSHOT& obb,
    const FCL_TRANSFORM& obbTransform,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

NTSTATUS DispatchObbSphere(
    const FCL_GEOMETRY_SNAPSHOT& obb,
    const FCL_TRANSFORM& obbTransform,
    const FCL_GEOMETRY_SNAPSHOT& sphere,
    const FCL_TRANSFORM& sphereTransform,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

NTSTATUS DispatchObbObb(
    const FCL_GEOMETRY_SNAPSHOT& objectA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& objectB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

NTSTATUS DispatchGjk(
    const FCL_GEOMETRY_SNAPSHOT& objectA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& objectB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

CollisionDispatchHandler g_DispatchMatrix[kGeometryTypeCount][kGeometryTypeCount] = {
    {nullptr, nullptr, nullptr, nullptr},
    {nullptr, DispatchSphereSphere, DispatchSphereObb, DispatchGjk},
    {nullptr, DispatchObbSphere, DispatchObbObb, DispatchGjk},
    {nullptr, DispatchGjk, DispatchGjk, DispatchGjk},
};

CollisionDispatchHandler ResolveDispatchHandler(
    FCL_GEOMETRY_TYPE lhs,
    FCL_GEOMETRY_TYPE rhs) noexcept {
    const size_t i = static_cast<size_t>(lhs);
    const size_t j = static_cast<size_t>(rhs);
    if (i >= kGeometryTypeCount || j >= kGeometryTypeCount) {
        return DispatchGjk;
    }
    CollisionDispatchHandler handler = g_DispatchMatrix[i][j];
    return (handler != nullptr) ? handler : DispatchGjk;
}

NTSTATUS InitializeCollisionObject(
    FCL_GEOMETRY_HANDLE handle,
    _In_opt_ const FCL_TRANSFORM* transform,
    _Out_ CollisionObject* object) noexcept {
    if (object == nullptr || !FclIsGeometryHandleValid(handle)) {
        return STATUS_INVALID_HANDLE;
    }

    object->Reference = {};
    object->Snapshot = {};
    object->Transform = (transform != nullptr) ? *transform : IdentityTransform();

    if (!IsValidTransform(object->Transform)) {
        return STATUS_INVALID_PARAMETER;
    }

    return FclAcquireGeometryReference(handle, &object->Reference, &object->Snapshot);
}

NTSTATUS DetectSphereSphere(
    const FCL_GEOMETRY_SNAPSHOT& sphereA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& sphereB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    const auto& descA = sphereA.Data.Sphere;
    const auto& descB = sphereB.Data.Sphere;

    if (descA.Radius <= 0.0f || descB.Radius <= 0.0f) {
        return STATUS_INVALID_PARAMETER;
    }

    const FCL_VECTOR3 worldCenterA = TransformPoint(transformA, descA.Center);
    const FCL_VECTOR3 worldCenterB = TransformPoint(transformB, descB.Center);

    const FCL_VECTOR3 delta = Subtract(worldCenterB, worldCenterA);
    const float distanceSquared = Dot(delta, delta);
    const float radiusSum = descA.Radius + descB.Radius;
    const float threshold = radiusSum * radiusSum + kCollisionTolerance;

    BOOLEAN colliding = FALSE;
    if (distanceSquared <= threshold) {
        colliding = TRUE;
    }

    *isColliding = colliding;

    if (contactInfo != nullptr) {
        RtlZeroMemory(contactInfo, sizeof(*contactInfo));
        if (colliding) {
            float distance = 0.0f;
            if (distanceSquared > 0.0f) {
                distance = static_cast<float>(sqrt(distanceSquared));
            }
            const float penetrationCandidate = radiusSum - distance;
            const float penetration = (penetrationCandidate > 0.0f) ? penetrationCandidate : 0.0f;
            FCL_VECTOR3 normal = {1.0f, 0.0f, 0.0f};
            if (distance > kSingularityThreshold) {
                normal = Scale(delta, 1.0f / distance);
            }
            contactInfo->Normal = normal;
            contactInfo->PenetrationDepth = penetration;
            contactInfo->PointOnObject1 = Add(worldCenterA, Scale(normal, descA.Radius));
            contactInfo->PointOnObject2 = Subtract(worldCenterB, Scale(normal, descB.Radius));
        }
    }

    return STATUS_SUCCESS;
}

struct OrientedBox {
    FCL_VECTOR3 Center;
    FCL_VECTOR3 Axes[3];
    FCL_VECTOR3 Extents;
};

OrientedBox BuildWorldObb(const FCL_OBB_GEOMETRY_DESC& desc, const FCL_TRANSFORM& transform) noexcept {
    OrientedBox box = {};
    const FCL_MATRIX3X3 combinedRotation = MultiplyMatrix(transform.Rotation, desc.Rotation);
    box.Center = TransformPoint(transform, desc.Center);
    for (int axis = 0; axis < 3; ++axis) {
        FCL_VECTOR3 vec = {combinedRotation.M[0][axis], combinedRotation.M[1][axis], combinedRotation.M[2][axis]};
        box.Axes[axis] = Normalize(vec);
        if (Length(box.Axes[axis]) <= kSingularityThreshold) {
            box.Axes[axis] = {0.0f, 0.0f, 0.0f};
        }
    }
    box.Extents = desc.Extents;
    return box;
}

float Clamp(float value, float minValue, float maxValue) noexcept {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

FCL_VECTOR3 ClosestPointOnObb(const OrientedBox& box, const FCL_VECTOR3& point) noexcept {
    FCL_VECTOR3 result = box.Center;
    FCL_VECTOR3 delta = Subtract(point, box.Center);
    for (int i = 0; i < 3; ++i) {
        const float distance = Dot(delta, box.Axes[i]);
        const float extent = (&box.Extents.X)[i];
        const float clamped = Clamp(distance, -extent, extent);
        const FCL_VECTOR3 contribution = Scale(box.Axes[i], clamped);
        result = Add(result, contribution);
    }
    return result;
}

FCL_VECTOR3 SupportPoint(const OrientedBox& box, const FCL_VECTOR3& direction) noexcept {
    FCL_VECTOR3 result = box.Center;
    for (int i = 0; i < 3; ++i) {
        float sign = Dot(direction, box.Axes[i]) >= 0.0f ? 1.0f : -1.0f;
        result = Add(result, Scale(box.Axes[i], sign * (&box.Extents.X)[i]));
    }
    return result;
}

NTSTATUS DetectSphereObb(
    const FCL_GEOMETRY_SNAPSHOT& sphere,
    const FCL_TRANSFORM& sphereTransform,
    const FCL_GEOMETRY_SNAPSHOT& obb,
    const FCL_TRANSFORM& obbTransform,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    const auto& sphereDesc = sphere.Data.Sphere;
    const auto& obbDesc = obb.Data.Obb;

    if (sphereDesc.Radius <= 0.0f) {
        return STATUS_INVALID_PARAMETER;
    }

    const FCL_VECTOR3 sphereCenter = TransformPoint(sphereTransform, sphereDesc.Center);
    const OrientedBox box = BuildWorldObb(obbDesc, obbTransform);

    const FCL_VECTOR3 closest = ClosestPointOnObb(box, sphereCenter);
    const FCL_VECTOR3 delta = Subtract(sphereCenter, closest);
    const float distanceSquared = Dot(delta, delta);
    BOOLEAN colliding = FALSE;
    if (distanceSquared <= (sphereDesc.Radius * sphereDesc.Radius + kCollisionTolerance)) {
        colliding = TRUE;
    }
    *isColliding = colliding;

    if (contactInfo != nullptr) {
        RtlZeroMemory(contactInfo, sizeof(*contactInfo));
        if (colliding) {
            float distance = static_cast<float>(sqrt(max(distanceSquared, 0.0f)));
            FCL_VECTOR3 normal = {1.0f, 0.0f, 0.0f};
            if (distance > kSingularityThreshold) {
                normal = Scale(delta, 1.0f / distance);
            }
            const float penetration = max(sphereDesc.Radius - distance, 0.0f);
            contactInfo->Normal = normal;
            contactInfo->PenetrationDepth = penetration;
            contactInfo->PointOnObject2 = closest;
            contactInfo->PointOnObject1 = Subtract(sphereCenter, Scale(normal, sphereDesc.Radius));
        }
    }

    return STATUS_SUCCESS;
}

bool TestAxis(
    float projection,
    float radiusSum,
    float& bestOverlap,
    FCL_VECTOR3 axisWorld,
    const FCL_VECTOR3& deltaCenter,
    _Out_ FCL_VECTOR3& bestAxis) noexcept {
    if (projection > radiusSum + kCollisionTolerance) {
        return false;
    }

    const float overlap = radiusSum - projection;
    if (overlap < bestOverlap) {
        bestOverlap = overlap;
        axisWorld = Normalize(axisWorld);
        if (Dot(axisWorld, deltaCenter) < 0.0f) {
            axisWorld = Scale(axisWorld, -1.0f);
        }
        bestAxis = axisWorld;
    }
    return true;
}

NTSTATUS DetectObbObb(
    const FCL_GEOMETRY_SNAPSHOT& obbA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& obbB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    const OrientedBox boxA = BuildWorldObb(obbA.Data.Obb, transformA);
    const OrientedBox boxB = BuildWorldObb(obbB.Data.Obb, transformB);

    float R[3][3];
    float AbsR[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = Dot(boxA.Axes[i], boxB.Axes[j]);
            AbsR[i][j] = fabs(R[i][j]) + kAxisThreshold;
        }
    }

    const FCL_VECTOR3 translationWorld = Subtract(boxB.Center, boxA.Center);
    const float t[3] = {
        Dot(translationWorld, boxA.Axes[0]),
        Dot(translationWorld, boxA.Axes[1]),
        Dot(translationWorld, boxA.Axes[2])};

    float bestOverlap = FLT_MAX;
    FCL_VECTOR3 bestAxis = {1.0f, 0.0f, 0.0f};

    // Axes of A
    for (int i = 0; i < 3; ++i) {
        const float ra = (&boxA.Extents.X)[i];
        float rb = (&boxB.Extents.X)[0] * AbsR[i][0] +
                   (&boxB.Extents.X)[1] * AbsR[i][1] +
                   (&boxB.Extents.X)[2] * AbsR[i][2];
        if (!TestAxis(fabs(t[i]), ra + rb, bestOverlap, boxA.Axes[i], translationWorld, bestAxis)) {
            *isColliding = FALSE;
            return STATUS_SUCCESS;
        }
    }

    // Axes of B
    for (int j = 0; j < 3; ++j) {
        const float rb = (&boxB.Extents.X)[j];
        float ra = (&boxA.Extents.X)[0] * AbsR[0][j] +
                   (&boxA.Extents.X)[1] * AbsR[1][j] +
                   (&boxA.Extents.X)[2] * AbsR[2][j];
        const float proj = fabs(Dot(translationWorld, boxB.Axes[j]));
        if (!TestAxis(proj, ra + rb, bestOverlap, boxB.Axes[j], translationWorld, bestAxis)) {
            *isColliding = FALSE;
            return STATUS_SUCCESS;
        }
    }

    // Axes from cross products
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            FCL_VECTOR3 axis = Cross(boxA.Axes[i], boxB.Axes[j]);
            if (Length(axis) <= kAxisThreshold) {
                continue;
            }

            const float ra = (&boxA.Extents.X)[(i + 1) % 3] * AbsR[(i + 2) % 3][j] +
                             (&boxA.Extents.X)[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
            const float rb = (&boxB.Extents.X)[(j + 1) % 3] * AbsR[i][(j + 2) % 3] +
                             (&boxB.Extents.X)[(j + 2) % 3] * AbsR[i][(j + 1) % 3];
            const float proj = fabs(t[(i + 1) % 3] * R[(i + 2) % 3][j] - t[(i + 2) % 3] * R[(i + 1) % 3][j]);

            if (!TestAxis(proj, ra + rb, bestOverlap, axis, translationWorld, bestAxis)) {
                *isColliding = FALSE;
                return STATUS_SUCCESS;
            }
        }
    }

    *isColliding = TRUE;

    if (contactInfo != nullptr) {
        FCL_VECTOR3 normal = Normalize(bestAxis);
        if (Length(normal) <= kSingularityThreshold) {
            normal = {1.0f, 0.0f, 0.0f};
        }
        contactInfo->Normal = normal;
        contactInfo->PenetrationDepth = bestOverlap;
        contactInfo->PointOnObject1 = SupportPoint(boxA, normal);
        contactInfo->PointOnObject2 = SupportPoint(boxB, Scale(normal, -1.0f));
    }

    return STATUS_SUCCESS;
}

NTSTATUS DispatchSphereSphere(
    const FCL_GEOMETRY_SNAPSHOT& objectA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& objectB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    return DetectSphereSphere(objectA, transformA, objectB, transformB, isColliding, contactInfo);
}

NTSTATUS DispatchSphereObb(
    const FCL_GEOMETRY_SNAPSHOT& sphere,
    const FCL_TRANSFORM& sphereTransform,
    const FCL_GEOMETRY_SNAPSHOT& obb,
    const FCL_TRANSFORM& obbTransform,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    return DetectSphereObb(sphere, sphereTransform, obb, obbTransform, isColliding, contactInfo);
}

NTSTATUS DispatchObbSphere(
    const FCL_GEOMETRY_SNAPSHOT& obb,
    const FCL_TRANSFORM& obbTransform,
    const FCL_GEOMETRY_SNAPSHOT& sphere,
    const FCL_TRANSFORM& sphereTransform,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    NTSTATUS status = DetectSphereObb(sphere, sphereTransform, obb, obbTransform, isColliding, contactInfo);
    if (NT_SUCCESS(status) && contactInfo != nullptr && isColliding != nullptr && *isColliding) {
        contactInfo->Normal = Scale(contactInfo->Normal, -1.0f);
        swap(contactInfo->PointOnObject1, contactInfo->PointOnObject2);
    }
    return status;
}

NTSTATUS DispatchObbObb(
    const FCL_GEOMETRY_SNAPSHOT& objectA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& objectB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    return DetectObbObb(objectA, transformA, objectB, transformB, isColliding, contactInfo);
}

NTSTATUS DispatchGjk(
    const FCL_GEOMETRY_SNAPSHOT& objectA,
    const FCL_TRANSFORM& transformA,
    const FCL_GEOMETRY_SNAPSHOT& objectB,
    const FCL_TRANSFORM& transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    return FclGjkIntersect(&objectA, &transformA, &objectB, &transformB, isColliding, contactInfo);
}

}  // namespace

extern "C"
NTSTATUS
FclCollisionDetect(
    _In_ FCL_GEOMETRY_HANDLE object1,
    _In_opt_ const FCL_TRANSFORM* transform1,
    _In_ FCL_GEOMETRY_HANDLE object2,
    _In_opt_ const FCL_TRANSFORM* transform2,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    if (isColliding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    *isColliding = FALSE;
    if (contactInfo != nullptr) {
        RtlZeroMemory(contactInfo, sizeof(*contactInfo));
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    CollisionObject objectA;
    NTSTATUS status = InitializeCollisionObject(object1, transform1, &objectA);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    CollisionObject objectB;
    status = InitializeCollisionObject(object2, transform2, &objectB);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    const CollisionDispatchHandler handler = ResolveDispatchHandler(objectA.Snapshot.Type, objectB.Snapshot.Type);
    return handler(
        objectA.Snapshot,
        objectA.Transform,
        objectB.Snapshot,
        objectB.Transform,
        isColliding,
        contactInfo);
}

extern "C"
NTSTATUS
FclCollideObjects(
    _In_ const FCL_COLLISION_OBJECT_DESC* object1,
    _In_ const FCL_COLLISION_OBJECT_DESC* object2,
    _In_opt_ const FCL_COLLISION_QUERY_REQUEST* request,
    _Out_ PFCL_COLLISION_QUERY_RESULT result) noexcept {
    if (object1 == nullptr || object2 == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    FCL_COLLISION_QUERY_REQUEST localRequest = {};
    if (request == nullptr) {
        localRequest.MaxContacts = 1;
        localRequest.EnableContactInfo = TRUE;
        request = &localRequest;
    }

    BOOLEAN isColliding = FALSE;
    FCL_CONTACT_INFO contact = {};
    PFCL_CONTACT_INFO contactPtr = nullptr;
    if (request->EnableContactInfo) {
        contactPtr = &contact;
    }

    NTSTATUS status = FclCollisionDetect(
        object1->Geometry,
        &object1->Transform,
        object2->Geometry,
        &object2->Transform,
        &isColliding,
        contactPtr);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    result->Intersecting = isColliding;
    result->ContactCount = (isColliding && contactPtr != nullptr) ? 1 : 0;
    if (result->ContactCount == 1) {
        result->Contact = contact;
    } else {
        RtlZeroMemory(&result->Contact, sizeof(result->Contact));
    }
    return STATUS_SUCCESS;
}




