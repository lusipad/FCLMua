#include <ntddk.h>

#include "fclmusa/collision.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/ioctl.h"

#include "device_control_demo.h"

using fclmusa::geom::IdentityTransform;

#ifdef FCL_MUSA_ENABLE_DEMO

NTSTATUS HandleSphereCollisionDemo(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_SPHERE_COLLISION_BUFFER) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_SPHERE_COLLISION_BUFFER)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* buffer = reinterpret_cast<FCL_SPHERE_COLLISION_BUFFER*>(irp->AssociatedIrp.SystemBuffer);
    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};

    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &buffer->SphereA, &sphereA);
    if (NT_SUCCESS(status)) {
        status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &buffer->SphereB, &sphereB);
    }
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphereA);
        return status;
    }

    FCL_COLLISION_OBJECT_DESC objA = {sphereA, IdentityTransform()};
    FCL_COLLISION_OBJECT_DESC objB = {sphereB, IdentityTransform()};

    FCL_COLLISION_QUERY_RESULT queryResult = {};
    status = FclCollideObjects(&objA, &objB, nullptr, &queryResult);
    if (NT_SUCCESS(status)) {
        buffer->Result.IsColliding = queryResult.Intersecting ? 1 : 0;
        buffer->Result.Contact = queryResult.Contact;
        irp->IoStatus.Information = sizeof(*buffer);
    }

    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return status;
}

#endif  // FCL_MUSA_ENABLE_DEMO

