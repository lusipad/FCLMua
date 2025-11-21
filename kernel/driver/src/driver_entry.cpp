#include <ntddk.h>

#include "fclmusa/driver.h"
#include "fclmusa/logging.h"
#include "fclmusa/version.h"

#include "periodic_scheduler.h"

EXTERN_C NTSTATUS FclDispatchDeviceControl(_In_ PDEVICE_OBJECT deviceObject, _Inout_ PIRP irp);

namespace {

VOID FclDriverUnload(_In_ PDRIVER_OBJECT /*driverObject*/);

NTSTATUS FclDispatchCreateClose(_In_ PDEVICE_OBJECT deviceObject, _Inout_ PIRP irp) {
    UNREFERENCED_PARAMETER(deviceObject);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

PDEVICE_OBJECT g_FclDeviceObject = nullptr;
UNICODE_STRING g_FclDeviceName = RTL_CONSTANT_STRING(FCL_MUSA_DEVICE_NAME);
UNICODE_STRING g_FclDosDeviceName = RTL_CONSTANT_STRING(FCL_MUSA_DOS_DEVICE_NAME);

NTSTATUS FclDriverCreateDevice(_In_ PDRIVER_OBJECT driverObject) {
    NTSTATUS status = IoCreateDevice(
        driverObject,
        0,
        &g_FclDeviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_FclDeviceObject);

    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("IoCreateDevice failed: 0x%X", status);
        return status;
    }

    status = IoCreateSymbolicLink(&g_FclDosDeviceName, &g_FclDeviceName);
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("IoCreateSymbolicLink failed: 0x%X", status);
        IoDeleteDevice(g_FclDeviceObject);
        g_FclDeviceObject = nullptr;
        return status;
    }

    driverObject->MajorFunction[IRP_MJ_CREATE] = FclDispatchCreateClose;
    driverObject->MajorFunction[IRP_MJ_CLOSE] = FclDispatchCreateClose;
    driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FclDispatchDeviceControl;

    driverObject->DriverUnload = FclDriverUnload;

    return STATUS_SUCCESS;
}

VOID FclDriverUnload(_In_ PDRIVER_OBJECT /*driverObject*/) {
    //
    // 先停止任何可能存在的周期性调度，确保内部线程退出，避免在
    // FclCleanup 之后仍有 Fcl* API 被调用。
    //
    FclPeriodicSchedulerShutdown();

    FclCleanup();
    if (g_FclDeviceObject != nullptr) {
        IoDeleteSymbolicLink(&g_FclDosDeviceName);
        IoDeleteDevice(g_FclDeviceObject);
        g_FclDeviceObject = nullptr;
    }
    FCL_LOG_INFO0("Driver unloaded");
}

}  // namespace

extern "C"
NTSTATUS
DriverMain(_In_ PDRIVER_OBJECT driverObject, _In_ PUNICODE_STRING registryPath) {
    UNREFERENCED_PARAMETER(registryPath);

    const auto* version = FclGetDriverVersion();
    FCL_LOG_INFO(
        "DriverEntry version %lu.%lu.%lu.%lu",
        version->Major,
        version->Minor,
        version->Patch,
        version->Build);

    NTSTATUS status = FclDriverCreateDevice(driverObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FclInitialize();
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("FclInitialize failed: 0x%X", status);
        IoDeleteSymbolicLink(&g_FclDosDeviceName);
        IoDeleteDevice(g_FclDeviceObject);
        g_FclDeviceObject = nullptr;
        return status;
    }

    FCL_LOG_INFO0("Driver initialized successfully");
    return STATUS_SUCCESS;
}
