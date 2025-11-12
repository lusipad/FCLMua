#include <ntddk.h>

#include "fclmusa/distance.h"
#include "fclmusa/driver.h"
#include "fclmusa/ioctl.h"
#include "fclmusa/logging.h"
#include "fclmusa/self_test.h"

namespace {

NTSTATUS HandlePing(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_PING_RESPONSE)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* response = reinterpret_cast<FCL_PING_RESPONSE*>(irp->AssociatedIrp.SystemBuffer);
    RtlZeroMemory(response, sizeof(*response));

    NTSTATUS status = FclQueryHealth(response);
    if (NT_SUCCESS(status)) {
        irp->IoStatus.Information = sizeof(*response);
    }

    return status;
}

NTSTATUS HandleSelfTest(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_SELF_TEST_RESULT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* response = reinterpret_cast<FCL_SELF_TEST_RESULT*>(irp->AssociatedIrp.SystemBuffer);
    RtlZeroMemory(response, sizeof(*response));

    NTSTATUS status = FclRunSelfTest(response);
    if (NT_SUCCESS(status)) {
        irp->IoStatus.Information = sizeof(*response);
    }
    return status;
}

NTSTATUS HandleCollisionQuery(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_COLLISION_IO_BUFFER) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_COLLISION_IO_BUFFER)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* buffer = reinterpret_cast<FCL_COLLISION_IO_BUFFER*>(irp->AssociatedIrp.SystemBuffer);
    const auto& query = buffer->Query;
    auto& result = buffer->Result;

    if (query.Object1.Value == 0 || query.Object2.Value == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    BOOLEAN isColliding = FALSE;
    FCL_CONTACT_INFO contact = {};
    NTSTATUS status = FclCollisionDetect(
        query.Object1,
        &query.Transform1,
        query.Object2,
        &query.Transform2,
        &isColliding,
        &contact);

    if (NT_SUCCESS(status)) {
        result.IsColliding = isColliding ? 1 : 0;
        if (isColliding) {
            result.Contact = contact;
        } else {
            RtlZeroMemory(&result.Contact, sizeof(result.Contact));
        }
        irp->IoStatus.Information = sizeof(FCL_COLLISION_IO_BUFFER);
    }

    return status;
}

NTSTATUS HandleDistanceQuery(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_DISTANCE_IO_BUFFER) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_DISTANCE_IO_BUFFER)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* buffer = reinterpret_cast<FCL_DISTANCE_IO_BUFFER*>(irp->AssociatedIrp.SystemBuffer);
    const auto& query = buffer->Query;
    auto& result = buffer->Result;

    if (query.Object1.Value == 0 || query.Object2.Value == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    FCL_DISTANCE_RESULT distance = {};
    NTSTATUS status = FclDistanceCompute(
        query.Object1,
        &query.Transform1,
        query.Object2,
        &query.Transform2,
        &distance);

    if (NT_SUCCESS(status)) {
        result.Distance = distance.Distance;
        result.ClosestPoint1 = distance.ClosestPoint1;
        result.ClosestPoint2 = distance.ClosestPoint2;
        irp->IoStatus.Information = sizeof(FCL_DISTANCE_IO_BUFFER);
    }

    return status;
}

NTSTATUS HandleCreateSphere(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_CREATE_SPHERE_INPUT) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_CREATE_SPHERE_OUTPUT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* input = reinterpret_cast<FCL_CREATE_SPHERE_INPUT*>(irp->AssociatedIrp.SystemBuffer);
    auto* output = reinterpret_cast<FCL_CREATE_SPHERE_OUTPUT*>(irp->AssociatedIrp.SystemBuffer);

    FCL_GEOMETRY_HANDLE handle = {};
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &input->Desc, &handle);
    if (NT_SUCCESS(status)) {
        output->Handle = handle;
        irp->IoStatus.Information = sizeof(*output);
    }
    return status;
}

NTSTATUS HandleDestroyGeometry(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_DESTROY_INPUT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    auto* input = reinterpret_cast<FCL_DESTROY_INPUT*>(irp->AssociatedIrp.SystemBuffer);
    return FclDestroyGeometry(input->Handle);
}

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

NTSTATUS HandleConvexCcdDemo(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_CONVEX_CCD_BUFFER) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_CONVEX_CCD_BUFFER)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* buffer = reinterpret_cast<FCL_CONVEX_CCD_BUFFER*>(irp->AssociatedIrp.SystemBuffer);
    if (!FclIsGeometryHandleValid(buffer->Object1) || !FclIsGeometryHandleValid(buffer->Object2)) {
        return STATUS_INVALID_HANDLE;
    }

    FCL_CONTINUOUS_COLLISION_QUERY query = {};
    query.Object1 = buffer->Object1;
    query.Object2 = buffer->Object2;
    query.Motion1 = buffer->Motion1;
    query.Motion2 = buffer->Motion2;
    query.Tolerance = 1e-4;
    query.MaxIterations = 64;

    NTSTATUS status = FclContinuousCollision(&query, &buffer->Result);
    if (NT_SUCCESS(status)) {
        irp->IoStatus.Information = sizeof(*buffer);
    }
    return status;
}

}  // namespace

extern "C"
NTSTATUS
FclDispatchDeviceControl(_In_ PDEVICE_OBJECT deviceObject, _Inout_ PIRP irp) {
    UNREFERENCED_PARAMETER(deviceObject);

    auto* stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    irp->IoStatus.Information = 0;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_FCL_PING:
            status = HandlePing(irp, stack);
            break;
        case IOCTL_FCL_SELF_TEST:
            status = HandleSelfTest(irp, stack);
            break;
        case IOCTL_FCL_QUERY_COLLISION:
            status = HandleCollisionQuery(irp, stack);
            break;
        case IOCTL_FCL_QUERY_DISTANCE:
            status = HandleDistanceQuery(irp, stack);
            break;
        case IOCTL_FCL_CREATE_SPHERE:
            status = HandleCreateSphere(irp, stack);
            break;
        case IOCTL_FCL_DESTROY_GEOMETRY:
            status = HandleDestroyGeometry(irp, stack);
            break;
        case IOCTL_FCL_SPHERE_COLLISION:
            status = HandleSphereCollisionDemo(irp, stack);
            break;
        case IOCTL_FCL_CONVEX_CCD:
            status = HandleConvexCcdDemo(irp, stack);
            break;
        default:
            FCL_LOG_WARN("Unsupported IOCTL: 0x%X", stack->Parameters.DeviceIoControl.IoControlCode);
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}
