#include <ntddk.h>
#include <ntintsafe.h>
#include <wdm.h>

#include "fclmusa/collision.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/logging.h"
#include "fclmusa/upstream_bridge.h"

namespace {

using namespace fclmusa::geom;

struct CollisionObject {
    FCL_GEOMETRY_REFERENCE Reference = {};
    FCL_GEOMETRY_SNAPSHOT Snapshot = {};
    FCL_TRANSFORM Transform = {};

    ~CollisionObject() {
        FclReleaseGeometryReference(&Reference);
    }
};

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

    return FclUpstreamCollide(
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
    PFCL_CONTACT_INFO contactPtr = request->EnableContactInfo ? &contact : nullptr;

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
