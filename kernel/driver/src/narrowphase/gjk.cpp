#include <ntddk.h>
#include <wdm.h>

#include <float.h>

#include <ccd/ccd.h>

#include "fclmusa/geometry/bvh_model.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/geometry/obb.h"
#include "fclmusa/geometry/obbrss.h"
#include "fclmusa/narrowphase/gjk.h"

namespace {

using namespace fclmusa::geom;

constexpr ccd_real_t kEpaTolerance = CCD_REAL(1.0e-4);
constexpr ccd_real_t kMprTolerance = CCD_REAL(1.0e-4);
constexpr ULONG kMaxIterations = 128;

struct LibccdObject;

using SupportDispatch = NTSTATUS (*)(
    const LibccdObject* object,
    const FCL_VECTOR3& direction,
    FCL_VECTOR3* result) noexcept;

using CenterDispatch = NTSTATUS (*)(
    const LibccdObject* object,
    FCL_VECTOR3* center) noexcept;

struct LibccdObject {
    const FCL_GEOMETRY_SNAPSHOT* Snapshot = nullptr;
    const FCL_TRANSFORM* Transform = nullptr;
    NTSTATUS Status = STATUS_SUCCESS;
    SupportDispatch Support = nullptr;
    CenterDispatch Center = nullptr;
};

bool TryGetRootObbrss(
    const FCL_GEOMETRY_SNAPSHOT* snapshot,
    FCL_OBBRSS* outVolume) noexcept {
    if (snapshot == nullptr || outVolume == nullptr) {
        return false;
    }

    if (snapshot->Type != FCL_GEOMETRY_MESH || snapshot->Data.Mesh.Bvh == nullptr) {
        return false;
    }

    ULONG nodeCount = 0;
    const FCL_BVH_NODE* nodes = FclBvhGetNodes(snapshot->Data.Mesh.Bvh, &nodeCount);
    if (nodes == nullptr || nodeCount == 0) {
        return false;
    }

    *outVolume = nodes[0].Volume;
    return true;
}

FCL_OBBRSS TransformObbrss(
    const FCL_OBBRSS& volume,
    const FCL_TRANSFORM& transform) noexcept {
    FCL_OBBRSS transformed = volume;
    transformed.Center = TransformPoint(transform, volume.Center);
    for (int axis = 0; axis < 3; ++axis) {
        transformed.Axis[axis] = MatrixVectorMultiply(transform.Rotation, volume.Axis[axis]);
        transformed.Axis[axis] = Normalize(transformed.Axis[axis]);
    }
    return transformed;
}

void WriteVector(const FCL_VECTOR3& source, ccd_vec3_t* destination) noexcept {
    destination->v[0] = source.X;
    destination->v[1] = source.Y;
    destination->v[2] = source.Z;
}

FCL_VECTOR3 ReadVector(const ccd_vec3_t& vector) noexcept {
    return {static_cast<float>(vector.v[0]), static_cast<float>(vector.v[1]), static_cast<float>(vector.v[2])};
}

FCL_VECTOR3 SupportSphere(const FCL_SPHERE_GEOMETRY_DESC& desc, const FCL_TRANSFORM& transform, const FCL_VECTOR3& direction) noexcept {
    FCL_VECTOR3 dir = direction;
    const float len = Length(dir);
    if (len <= kSingularityEpsilon) {
        dir = {1.0f, 0.0f, 0.0f};
    } else {
        dir = Scale(dir, 1.0f / len);
    }
    const FCL_VECTOR3 center = TransformPoint(transform, desc.Center);
    return Add(center, Scale(dir, desc.Radius));
}

FCL_VECTOR3 SupportObb(const FCL_OBB_GEOMETRY_DESC& desc, const FCL_TRANSFORM& transform, const FCL_VECTOR3& direction) noexcept {
    const OrientedBox box = BuildWorldObb(desc, transform);
    FCL_VECTOR3 result = box.Center;
    for (int i = 0; i < 3; ++i) {
        const float extent = (&box.Extents.X)[i];
        const float sign = Dot(direction, box.Axes[i]) >= 0.0f ? 1.0f : -1.0f;
        result = Add(result, Scale(box.Axes[i], sign * extent));
    }
    return result;
}

FCL_VECTOR3 SupportMesh(const FCL_GEOMETRY_SNAPSHOT& snapshot, const FCL_TRANSFORM& transform, const FCL_VECTOR3& direction) noexcept {
    const auto& mesh = snapshot.Data.Mesh;
    if (mesh.Vertices == nullptr || mesh.VertexCount == 0) {
        return TransformPoint(transform, {0.0f, 0.0f, 0.0f});
    }
    float bestDot = -FLT_MAX;
    FCL_VECTOR3 bestPoint = TransformPoint(transform, mesh.Vertices[0]);
    for (ULONG i = 0; i < mesh.VertexCount; ++i) {
        const FCL_VECTOR3 vertexWorld = TransformPoint(transform, mesh.Vertices[i]);
        const float dot = Dot(vertexWorld, direction);
        if (dot > bestDot) {
            bestDot = dot;
            bestPoint = vertexWorld;
        }
    }
    return bestPoint;
}

NTSTATUS SupportGeneric(
    const FCL_GEOMETRY_SNAPSHOT* snapshot,
    const FCL_TRANSFORM* transform,
    const FCL_VECTOR3& direction,
    FCL_VECTOR3* result) noexcept {
    if (snapshot == nullptr || transform == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (snapshot->Type) {
        case FCL_GEOMETRY_SPHERE:
            *result = SupportSphere(snapshot->Data.Sphere, *transform, direction);
            return STATUS_SUCCESS;
        case FCL_GEOMETRY_OBB:
            *result = SupportObb(snapshot->Data.Obb, *transform, direction);
            return STATUS_SUCCESS;
        case FCL_GEOMETRY_MESH:
            *result = SupportMesh(*snapshot, *transform, direction);
            return STATUS_SUCCESS;
        default:
            return STATUS_NOT_SUPPORTED;
    }
}

NTSTATUS ComputeCenterGeneric(
    const FCL_GEOMETRY_SNAPSHOT* snapshot,
    const FCL_TRANSFORM* transform,
    FCL_VECTOR3* center) noexcept {
    if (snapshot == nullptr || transform == nullptr || center == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (snapshot->Type) {
        case FCL_GEOMETRY_SPHERE:
            *center = TransformPoint(*transform, snapshot->Data.Sphere.Center);
            return STATUS_SUCCESS;
        case FCL_GEOMETRY_OBB: {
            const OrientedBox box = BuildWorldObb(snapshot->Data.Obb, *transform);
            *center = box.Center;
            return STATUS_SUCCESS;
        }
        case FCL_GEOMETRY_MESH: {
            const auto& mesh = snapshot->Data.Mesh;
            if (mesh.Vertices == nullptr || mesh.VertexCount == 0) {
                *center = TransformPoint(*transform, {0.0f, 0.0f, 0.0f});
                return STATUS_SUCCESS;
            }
            FCL_VECTOR3 accumulator = {0.0f, 0.0f, 0.0f};
            for (ULONG i = 0; i < mesh.VertexCount; ++i) {
                accumulator = Add(accumulator, TransformPoint(*transform, mesh.Vertices[i]));
            }
            const float scale = 1.0f / static_cast<float>(mesh.VertexCount);
            *center = Scale(accumulator, scale);
            return STATUS_SUCCESS;
        }
        default:
            return STATUS_NOT_SUPPORTED;
    }
}

NTSTATUS SupportSphereDispatch(
    const LibccdObject* object,
    const FCL_VECTOR3& direction,
    FCL_VECTOR3* result) noexcept {
    if (object == nullptr || object->Snapshot == nullptr || object->Transform == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *result = SupportSphere(object->Snapshot->Data.Sphere, *object->Transform, direction);
    return STATUS_SUCCESS;
}

NTSTATUS SupportObbDispatch(
    const LibccdObject* object,
    const FCL_VECTOR3& direction,
    FCL_VECTOR3* result) noexcept {
    if (object == nullptr || object->Snapshot == nullptr || object->Transform == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *result = SupportObb(object->Snapshot->Data.Obb, *object->Transform, direction);
    return STATUS_SUCCESS;
}

NTSTATUS SupportMeshDispatch(
    const LibccdObject* object,
    const FCL_VECTOR3& direction,
    FCL_VECTOR3* result) noexcept {
    if (object == nullptr || object->Snapshot == nullptr || object->Transform == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *result = SupportMesh(*object->Snapshot, *object->Transform, direction);
    return STATUS_SUCCESS;
}

NTSTATUS SupportFallbackDispatch(
    const LibccdObject* object,
    const FCL_VECTOR3& direction,
    FCL_VECTOR3* result) noexcept {
    if (object == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    return SupportGeneric(object->Snapshot, object->Transform, direction, result);
}

NTSTATUS CenterSphereDispatch(
    const LibccdObject* object,
    FCL_VECTOR3* center) noexcept {
    if (object == nullptr || object->Snapshot == nullptr || object->Transform == nullptr || center == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *center = TransformPoint(*object->Transform, object->Snapshot->Data.Sphere.Center);
    return STATUS_SUCCESS;
}

NTSTATUS CenterObbDispatch(
    const LibccdObject* object,
    FCL_VECTOR3* center) noexcept {
    if (object == nullptr || object->Snapshot == nullptr || object->Transform == nullptr || center == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    const OrientedBox box = BuildWorldObb(object->Snapshot->Data.Obb, *object->Transform);
    *center = box.Center;
    return STATUS_SUCCESS;
}

NTSTATUS CenterMeshDispatch(
    const LibccdObject* object,
    FCL_VECTOR3* center) noexcept {
    if (object == nullptr || object->Snapshot == nullptr || object->Transform == nullptr || center == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    const auto& mesh = object->Snapshot->Data.Mesh;
    if (mesh.Vertices == nullptr || mesh.VertexCount == 0) {
        *center = TransformPoint(*object->Transform, {0.0f, 0.0f, 0.0f});
        return STATUS_SUCCESS;
    }
    FCL_VECTOR3 accumulator = {0.0f, 0.0f, 0.0f};
    for (ULONG i = 0; i < mesh.VertexCount; ++i) {
        accumulator = Add(accumulator, TransformPoint(*object->Transform, mesh.Vertices[i]));
    }
    const float scale = 1.0f / static_cast<float>(mesh.VertexCount);
    *center = Scale(accumulator, scale);
    return STATUS_SUCCESS;
}

NTSTATUS CenterFallbackDispatch(
    const LibccdObject* object,
    FCL_VECTOR3* center) noexcept {
    if (object == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    return ComputeCenterGeneric(object->Snapshot, object->Transform, center);
}

NTSTATUS InitializeLibccdObject(LibccdObject* object) noexcept {
    if (object == nullptr || object->Snapshot == nullptr || object->Transform == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    object->Support = nullptr;
    object->Center = nullptr;
    object->Status = STATUS_SUCCESS;

    switch (object->Snapshot->Type) {
        case FCL_GEOMETRY_SPHERE:
            object->Support = SupportSphereDispatch;
            object->Center = CenterSphereDispatch;
            break;
        case FCL_GEOMETRY_OBB:
            object->Support = SupportObbDispatch;
            object->Center = CenterObbDispatch;
            break;
        case FCL_GEOMETRY_MESH:
            object->Support = SupportMeshDispatch;
            object->Center = CenterMeshDispatch;
            break;
        default:
            object->Support = SupportFallbackDispatch;
            object->Center = CenterFallbackDispatch;
            return STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}

void RecordFailure(LibccdObject* object, NTSTATUS status) noexcept {
    if (NT_SUCCESS(object->Status)) {
        object->Status = status;
    }
}

void FclCcdSupportCallback(const void* objectPtr, const ccd_vec3_t* direction, ccd_vec3_t* result) {
    auto* object = const_cast<LibccdObject*>(static_cast<const LibccdObject*>(objectPtr));
    if (!NT_SUCCESS(object->Status)) {
        ccdVec3Set(result, CCD_ZERO, CCD_ZERO, CCD_ZERO);
        return;
    }

    FCL_VECTOR3 request = ReadVector(*direction);
    if (Length(request) <= kSingularityEpsilon) {
        request = {1.0f, 0.0f, 0.0f};
    }

    FCL_VECTOR3 support = {};
    if (object->Support == nullptr) {
        RecordFailure(object, STATUS_NOT_SUPPORTED);
        ccdVec3Set(result, CCD_ZERO, CCD_ZERO, CCD_ZERO);
        return;
    }
    const NTSTATUS status = object->Support(object, request, &support);
    if (!NT_SUCCESS(status)) {
        RecordFailure(object, status);
        ccdVec3Set(result, CCD_ZERO, CCD_ZERO, CCD_ZERO);
        return;
    }

    WriteVector(support, result);
}

void FclCcdCenterCallback(const void* objectPtr, ccd_vec3_t* center) {
    auto* object = const_cast<LibccdObject*>(static_cast<const LibccdObject*>(objectPtr));
    if (!NT_SUCCESS(object->Status)) {
        ccdVec3Set(center, CCD_ZERO, CCD_ZERO, CCD_ZERO);
        return;
    }

    if (object->Center == nullptr) {
        RecordFailure(object, STATUS_NOT_SUPPORTED);
        ccdVec3Set(center, CCD_ZERO, CCD_ZERO, CCD_ZERO);
        return;
    }

    FCL_VECTOR3 worldCenter = {};
    const NTSTATUS status = object->Center(object, &worldCenter);
    if (!NT_SUCCESS(status)) {
        RecordFailure(object, status);
        ccdVec3Set(center, CCD_ZERO, CCD_ZERO, CCD_ZERO);
        return;
    }

    WriteVector(worldCenter, center);
}

}  // namespace

extern "C"
NTSTATUS
FclGjkIntersect(
    _In_ const FCL_GEOMETRY_SNAPSHOT* shapeA,
    _In_ const FCL_TRANSFORM* transformA,
    _In_ const FCL_GEOMETRY_SNAPSHOT* shapeB,
    _In_ const FCL_TRANSFORM* transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    if (isColliding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    if (contactInfo != nullptr) {
        RtlZeroMemory(contactInfo, sizeof(*contactInfo));
    }

    *isColliding = FALSE;

    if (shapeA == nullptr || shapeB == nullptr || transformA == nullptr || transformB == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    LibccdObject objectA = {shapeA, transformA, STATUS_SUCCESS};
    LibccdObject objectB = {shapeB, transformB, STATUS_SUCCESS};

    NTSTATUS status = InitializeLibccdObject(&objectA);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = InitializeLibccdObject(&objectB);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ccd_t ccd = {};
    CCD_INIT(&ccd);
    ccd.first_dir = ccdFirstDirDefault;
    ccd.support1 = FclCcdSupportCallback;
    ccd.support2 = FclCcdSupportCallback;
    ccd.center1 = FclCcdCenterCallback;
    ccd.center2 = FclCcdCenterCallback;
    ccd.max_iterations = kMaxIterations;
    ccd.epa_tolerance = kEpaTolerance;
    ccd.mpr_tolerance = kMprTolerance;

    FCL_OBBRSS volumeA = {};
    FCL_OBBRSS volumeB = {};
    if (TryGetRootObbrss(shapeA, &volumeA) && TryGetRootObbrss(shapeB, &volumeB)) {
        const FCL_OBBRSS transformedA = TransformObbrss(volumeA, *transformA);
        const FCL_OBBRSS transformedB = TransformObbrss(volumeB, *transformB);
        if (!FclObbrssOverlap(&transformedA, &transformedB)) {
            *isColliding = FALSE;
            return STATUS_SUCCESS;
        }
    }

    const int intersecting = ccdGJKIntersect(&objectA, &objectB, &ccd);

    if (!NT_SUCCESS(objectA.Status)) {
        return objectA.Status;
    }
    if (!NT_SUCCESS(objectB.Status)) {
        return objectB.Status;
    }

    *isColliding = intersecting ? TRUE : FALSE;

    if (*isColliding && contactInfo != nullptr) {
        ccd_real_t depth = CCD_ZERO;
        ccd_vec3_t dir = {};
        ccd_vec3_t pos = {};
        const int penetration = ccdMPRPenetration(&objectA, &objectB, &ccd, &depth, &dir, &pos);
        if (penetration == 0) {
            const FCL_VECTOR3 normal = ReadVector(dir);
            const float depthFloat = static_cast<float>(depth);
            contactInfo->Normal = normal;
            contactInfo->PenetrationDepth = depthFloat;
            contactInfo->PointOnObject1 = ReadVector(pos);
            contactInfo->PointOnObject2 = Subtract(contactInfo->PointOnObject1, Scale(normal, depthFloat));
        }
    }

    return STATUS_SUCCESS;
}
