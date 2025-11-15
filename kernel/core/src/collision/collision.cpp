#include <ntddk.h>
#include <ntintsafe.h>
#include <wdm.h>

#include "fclmusa/collision.h"
#include "fclmusa/driver.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/logging.h"
#include "fclmusa/upstream/upstream_bridge.h"

namespace {

using namespace fclmusa::geom;

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
FclCollisionCoreFromSnapshots(
    _In_ const FCL_GEOMETRY_SNAPSHOT* object1,
    _In_ const FCL_TRANSFORM* transform1,
    _In_ const FCL_GEOMETRY_SNAPSHOT* object2,
    _In_ const FCL_TRANSFORM* transform2,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    if (isColliding == nullptr || object1 == nullptr || object2 == nullptr || transform1 == nullptr || transform2 == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    *isColliding = FALSE;
    if (contactInfo != nullptr) {
        RtlZeroMemory(contactInfo, sizeof(*contactInfo));
    }

    const ULONGLONG start = QueryTimeMicroseconds();
    NTSTATUS status = FclUpstreamCollide(
        *object1,
        *transform1,
        *object2,
        *transform2,
        isColliding,
        contactInfo);
    const ULONGLONG end = QueryTimeMicroseconds();

    if (NT_SUCCESS(status) && start != 0 && end != 0) {
        const ULONGLONG elapsed = AbsoluteDifference(end, start);
        if (elapsed != 0) {
            FclDiagnosticsRecordCollisionDuration(elapsed);
            if (KeGetCurrentIrql() == DISPATCH_LEVEL) {
                FclDiagnosticsRecordDpcCollisionDuration(elapsed);
            }
        }
    }

    return status;
}

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

    return FclCollisionCoreFromSnapshots(
        &objectA.Snapshot,
        &objectA.Transform,
        &objectB.Snapshot,
        &objectB.Transform,
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
