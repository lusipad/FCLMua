#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ntddk.h>
#include <wdm.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "fclmusa/broadphase.h"
#include "fclmusa/geometry/bvh_model.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/geometry/obb.h"
#include "fclmusa/geometry/obbrss.h"
#include "fclmusa/logging.h"

namespace {

using namespace fclmusa::geom;

struct Aabb {
    FCL_VECTOR3 Min;
    FCL_VECTOR3 Max;
};

struct ObjectEntry {
    FCL_GEOMETRY_HANDLE Handle;
    FCL_GEOMETRY_REFERENCE Reference;
    FCL_GEOMETRY_SNAPSHOT Snapshot;
    FCL_TRANSFORM Transform;
    Aabb Bounds;
};

bool Overlaps(const Aabb& lhs, const Aabb& rhs) noexcept {
    if (lhs.Max.X < rhs.Min.X || rhs.Max.X < lhs.Min.X) {
        return false;
    }
    if (lhs.Max.Y < rhs.Min.Y || rhs.Max.Y < lhs.Min.Y) {
        return false;
    }
    if (lhs.Max.Z < rhs.Min.Z || rhs.Max.Z < lhs.Min.Z) {
        return false;
    }
    return true;
}

Aabb SphereAabb(const FCL_SPHERE_GEOMETRY_DESC& desc, const FCL_TRANSFORM& transform) noexcept {
    const FCL_VECTOR3 center = TransformPoint(transform, desc.Center);
    const FCL_VECTOR3 radius = {desc.Radius, desc.Radius, desc.Radius};
    Aabb box = {};
    box.Min = Subtract(center, radius);
    box.Max = Add(center, radius);
    return box;
}

FCL_VECTOR3 AbsAxisScaled(const FCL_VECTOR3& axis, float extent) noexcept {
    return {fabs(axis.X) * extent, fabs(axis.Y) * extent, fabs(axis.Z) * extent};
}

Aabb ObbAabb(const FCL_OBB_GEOMETRY_DESC& desc, const FCL_TRANSFORM& transform) noexcept {
    const OrientedBox box = BuildWorldObb(desc, transform);
    FCL_VECTOR3 offset = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; ++i) {
        const float extent = (&box.Extents.X)[i];
        const FCL_VECTOR3 axis = box.Axes[i];
        const FCL_VECTOR3 contribution = AbsAxisScaled(axis, extent);
        offset = Add(offset, contribution);
    }
    Aabb bounds = {};
    bounds.Min = Subtract(box.Center, offset);
    bounds.Max = Add(box.Center, offset);
    return bounds;
}

FCL_OBBRSS TransformObbrss(
    const FCL_OBBRSS& volume,
    const FCL_TRANSFORM& transform) noexcept {
    FCL_OBBRSS transformed = volume;
    transformed.Center = TransformPoint(transform, volume.Center);
    for (int axis = 0; axis < 3; ++axis) {
        FCL_VECTOR3 worldAxis = MatrixVectorMultiply(transform.Rotation, volume.Axis[axis]);
        const float length = Length(worldAxis);
        if (length > kSingularityEpsilon) {
            worldAxis = Scale(worldAxis, 1.0f / length);
        }
        transformed.Axis[axis] = worldAxis;
    }
    return transformed;
}

Aabb ObbrssAabb(const FCL_OBBRSS& volume) noexcept {
    FCL_VECTOR3 offset = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; ++i) {
        const float extent = (&volume.Extents.X)[i];
        const FCL_VECTOR3 contribution = AbsAxisScaled(volume.Axis[i], extent);
        offset = Add(offset, contribution);
    }
    Aabb bounds = {};
    bounds.Min = Subtract(volume.Center, offset);
    bounds.Max = Add(volume.Center, offset);
    return bounds;
}

bool TryGetMeshVolume(
    const FCL_GEOMETRY_SNAPSHOT& snapshot,
    FCL_OBBRSS* volume) noexcept {
    if (snapshot.Type != FCL_GEOMETRY_MESH || snapshot.Data.Mesh.Bvh == nullptr || volume == nullptr) {
        return false;
    }

    ULONG nodeCount = 0;
    const FCL_BVH_NODE* nodes = FclBvhGetNodes(snapshot.Data.Mesh.Bvh, &nodeCount);
    if (nodes == nullptr || nodeCount == 0) {
        return false;
    }

    *volume = nodes[0].Volume;
    return true;
}

Aabb MeshVerticesAabb(
    const FCL_GEOMETRY_SNAPSHOT& snapshot,
    const FCL_TRANSFORM& transform) noexcept {
    const auto& mesh = snapshot.Data.Mesh;
    Aabb bounds = {};
    if (mesh.Vertices == nullptr || mesh.VertexCount == 0) {
        const FCL_VECTOR3 origin = TransformPoint(transform, {0.0f, 0.0f, 0.0f});
        bounds.Min = origin;
        bounds.Max = origin;
        return bounds;
    }

    FCL_VECTOR3 vertex = TransformPoint(transform, mesh.Vertices[0]);
    bounds.Min = vertex;
    bounds.Max = vertex;
    for (ULONG i = 1; i < mesh.VertexCount; ++i) {
        vertex = TransformPoint(transform, mesh.Vertices[i]);
        bounds.Min.X = std::min(bounds.Min.X, vertex.X);
        bounds.Min.Y = std::min(bounds.Min.Y, vertex.Y);
        bounds.Min.Z = std::min(bounds.Min.Z, vertex.Z);
        bounds.Max.X = std::max(bounds.Max.X, vertex.X);
        bounds.Max.Y = std::max(bounds.Max.Y, vertex.Y);
        bounds.Max.Z = std::max(bounds.Max.Z, vertex.Z);
    }
    return bounds;
}

Aabb MeshAabb(
    const FCL_GEOMETRY_SNAPSHOT& snapshot,
    const FCL_TRANSFORM& transform) noexcept {
    FCL_OBBRSS volume = {};
    if (TryGetMeshVolume(snapshot, &volume)) {
        const FCL_OBBRSS transformed = TransformObbrss(volume, transform);
        return ObbrssAabb(transformed);
    }
    return MeshVerticesAabb(snapshot, transform);
}

NTSTATUS BuildEntry(
    const FCL_BROADPHASE_OBJECT& object,
    ObjectEntry* entry) noexcept {
    entry->Handle = object.Handle;
    entry->Reference = {};
    entry->Snapshot = {};
    entry->Transform = object.Transform != nullptr ? *object.Transform : IdentityTransform();

    NTSTATUS status = FclAcquireGeometryReference(object.Handle, &entry->Reference, &entry->Snapshot);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    switch (entry->Snapshot.Type) {
        case FCL_GEOMETRY_SPHERE:
            entry->Bounds = SphereAabb(entry->Snapshot.Data.Sphere, entry->Transform);
            break;
        case FCL_GEOMETRY_OBB:
            entry->Bounds = ObbAabb(entry->Snapshot.Data.Obb, entry->Transform);
            break;
        case FCL_GEOMETRY_MESH:
            entry->Bounds = MeshAabb(entry->Snapshot, entry->Transform);
            break;
        default:
            FclReleaseGeometryReference(&entry->Reference);
            return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclBroadphaseDetect(
    _In_reads_(objectCount) const FCL_BROADPHASE_OBJECT* objects,
    _In_ ULONG objectCount,
    _Out_writes_opt_(pairCapacity) PFCL_BROADPHASE_PAIR pairs,
    _In_ ULONG pairCapacity,
    _Out_ PULONG pairCount) noexcept {
    if (pairCount == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *pairCount = 0;

    if (objects == nullptr && objectCount > 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    std::vector<ObjectEntry> entries;
    entries.reserve(objectCount);

    for (ULONG i = 0; i < objectCount; ++i) {
        ObjectEntry entry = {};
        NTSTATUS status = BuildEntry(objects[i], &entry);
        if (!NT_SUCCESS(status)) {
            for (auto& built : entries) {
                FclReleaseGeometryReference(&built.Reference);
            }
            return status;
        }
        entries.emplace_back(entry);
    }

    ULONG writeIndex = 0;
    for (ULONG i = 0; i < entries.size(); ++i) {
        for (ULONG j = i + 1; j < entries.size(); ++j) {
            if (!Overlaps(entries[i].Bounds, entries[j].Bounds)) {
                continue;
            }
            if (pairs != nullptr && writeIndex < pairCapacity) {
                pairs[writeIndex].A = entries[i].Handle;
                pairs[writeIndex].B = entries[j].Handle;
            }
            ++writeIndex;
        }
    }

    *pairCount = writeIndex;

    for (auto& entry : entries) {
        FclReleaseGeometryReference(&entry.Reference);
    }

    if (pairs != nullptr && writeIndex > pairCapacity) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}
