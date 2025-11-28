#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fclmusa/platform.h"

#include <memory>
#include <vector>

#include <fcl/broadphase/broadphase_dynamic_AABB_tree.h>

#include "fclmusa/broadphase.h"
#include "fclmusa/geometry/math_utils.h"

#include "fclmusa/upstream/geometry_bridge.h"

namespace {

using fclmusa::upstream::CombineTransforms;
using fclmusa::upstream::GeometryBinding;
using fclmusa::upstream::ToEigenTransform;
using fclmusa::upstream::BuildGeometryBinding;

struct ManagedObject {
    FCL_GEOMETRY_HANDLE Handle = {};
    FCL_GEOMETRY_REFERENCE Reference = {};
    GeometryBinding Binding = {};
    fcl::Transform3d WorldTransform = fcl::Transform3d::Identity();
    std::unique_ptr<fcl::CollisionObjectd> CollisionObject;
};

struct PairCollector {
    PFCL_BROADPHASE_PAIR Buffer;
    ULONG Capacity;
    ULONG Count;
};

bool CollisionCallback(
    fcl::CollisionObjectd* objectA,
    fcl::CollisionObjectd* objectB,
    void* context) {
    if (objectA == nullptr || objectB == nullptr || context == nullptr) {
        return false;
    }
    auto* collector = static_cast<PairCollector*>(context);
    auto* managedA = static_cast<ManagedObject*>(objectA->getUserData());
    auto* managedB = static_cast<ManagedObject*>(objectB->getUserData());
    if (managedA == nullptr || managedB == nullptr) {
        return false;
    }
    if (collector->Buffer != nullptr && collector->Count < collector->Capacity) {
        collector->Buffer[collector->Count].A = managedA->Handle;
        collector->Buffer[collector->Count].B = managedB->Handle;
    }
    ++collector->Count;
    return false;
}

NTSTATUS BuildManagedObject(
    const FCL_BROADPHASE_OBJECT& source,
    ManagedObject* target) noexcept {
    if (target == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    target->Handle = source.Handle;
    target->Reference = {};

    FCL_GEOMETRY_SNAPSHOT snapshot = {};
    NTSTATUS status = FclAcquireGeometryReference(source.Handle, &target->Reference, &snapshot);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = BuildGeometryBinding(snapshot, &target->Binding);
    if (!NT_SUCCESS(status)) {
        FclReleaseGeometryReference(&target->Reference);
        return status;
    }

    const FCL_TRANSFORM inputTransform = (source.Transform != nullptr)
        ? *source.Transform
        : fclmusa::geom::IdentityTransform();
    const FCL_TRANSFORM worldTransform = CombineTransforms(inputTransform, target->Binding.LocalTransform);

    try {
        target->WorldTransform = ToEigenTransform(worldTransform);
        target->CollisionObject = std::make_unique<fcl::CollisionObjectd>(
            target->Binding.Geometry,
            target->WorldTransform);
        target->CollisionObject->setUserData(target);
    } catch (const std::bad_alloc&) {
        FclReleaseGeometryReference(&target->Reference);
        return STATUS_INSUFFICIENT_RESOURCES;
    } catch (...) {
        FclReleaseGeometryReference(&target->Reference);
        return STATUS_INTERNAL_ERROR;
    }

    return STATUS_SUCCESS;
}

void ReleaseManagedObjects(std::vector<ManagedObject>& objects) noexcept {
    for (auto& entry : objects) {
        FclReleaseGeometryReference(&entry.Reference);
    }
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

    std::vector<ManagedObject> managedObjects;
    try {
        managedObjects.reserve(objectCount);
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (ULONG i = 0; i < objectCount; ++i) {
        ManagedObject entry = {};
        NTSTATUS status = BuildManagedObject(objects[i], &entry);
        if (!NT_SUCCESS(status)) {
            ReleaseManagedObjects(managedObjects);
            return status;
        }
        managedObjects.emplace_back(std::move(entry));
    }

    std::vector<fcl::CollisionObjectd*> collisionObjects;
    collisionObjects.reserve(managedObjects.size());
    for (auto& entry : managedObjects) {
        collisionObjects.push_back(entry.CollisionObject.get());
    }

    fcl::DynamicAABBTreeCollisionManagerd manager;
    manager.registerObjects(collisionObjects);
    manager.setup();

    PairCollector collector = {pairs, pairCapacity, 0};
    manager.collide(&collector, &CollisionCallback);

    *pairCount = collector.Count;

    ReleaseManagedObjects(managedObjects);

    if (pairs != nullptr && collector.Count > pairCapacity) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

