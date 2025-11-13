#include <ntddk.h>
#include <ntintsafe.h>
#include <wdm.h>

#include "fclmusa/distance.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/upstream/upstream_bridge.h"

namespace {

using namespace fclmusa::geom;

struct DistanceObject {
    FCL_GEOMETRY_REFERENCE Reference = {};
    FCL_GEOMETRY_SNAPSHOT Snapshot = {};
    FCL_TRANSFORM Transform = {};

    ~DistanceObject() {
        FclReleaseGeometryReference(&Reference);
    }
};

NTSTATUS InitializeDistanceObject(
    FCL_GEOMETRY_HANDLE handle,
    _In_opt_ const FCL_TRANSFORM* transform,
    _Out_ DistanceObject* object) noexcept {
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

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    DistanceObject objectA;
    NTSTATUS status = InitializeDistanceObject(object1, transform1, &objectA);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    DistanceObject objectB;
    status = InitializeDistanceObject(object2, transform2, &objectB);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return FclUpstreamDistance(
        objectA.Snapshot,
        objectA.Transform,
        objectB.Snapshot,
        objectB.Transform,
        result);
}
