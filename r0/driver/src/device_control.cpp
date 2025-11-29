#include <ntddk.h>
#include <ntintsafe.h>

#include "fclmusa/distance.h"
#include "fclmusa/driver.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/ioctl.h"
#include "fclmusa/logging.h"
#include "fclmusa/self_test.h"

#ifdef FCL_MUSA_ENABLE_DEMO
#include "device_control_demo.h"
#endif

// 注意：本文件仅作为 IOCTL 与 FCL 管理模块之间的薄封装层。
// 只负责 IRP/缓冲区编解码并调用 Fcl* API，不引入几何/碰撞/CCD 控制逻辑或持久状态。

using fclmusa::geom::IdentityTransform;

namespace {

// PushLock 兼容辅助（与 core/driver_state.cpp 一致）
#ifndef ExEnterCriticalRegionAndAcquirePushLockExclusive
inline VOID ExEnterCriticalRegionAndAcquirePushLockExclusive(PEX_PUSH_LOCK PushLock) {
    KeEnterCriticalRegion();
    ExAcquirePushLockExclusive(PushLock);
}
#endif

#ifndef ExReleasePushLockExclusiveAndLeaveCriticalRegion
inline VOID ExReleasePushLockExclusiveAndLeaveCriticalRegion(PEX_PUSH_LOCK PushLock) {
    ExReleasePushLockExclusive(PushLock);
    KeLeaveCriticalRegion();
}
#endif

#ifndef ExEnterCriticalRegionAndAcquirePushLockShared
inline VOID ExEnterCriticalRegionAndAcquirePushLockShared(PEX_PUSH_LOCK PushLock) {
    KeEnterCriticalRegion();
    ExAcquirePushLockShared(PushLock);
}
#endif

#ifndef ExReleasePushLockSharedAndLeaveCriticalRegion
inline VOID ExReleasePushLockSharedAndLeaveCriticalRegion(PEX_PUSH_LOCK PushLock) {
    ExReleasePushLockShared(PushLock);
    KeLeaveCriticalRegion();
}
#endif

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

constexpr ULONGLONG kPeriodicDpcBudgetUs = 100;

struct FCL_PERIODIC_COLLISION_STATE {
    EX_PUSH_LOCK Lock;
    BOOLEAN Enabled;
    UCHAR Reserved[3];
    FCL_PERIODIC_COLLISION_CONFIG Config;
    ULONG InnerIterations;
    volatile ULONGLONG Sequence;
    NTSTATUS LastStatus;
    FCL_COLLISION_RESULT LastResult;

    KTIMER Timer;
    KDPC TimerDpc;
    KEVENT DpcIdleEvent;
    volatile LONG DpcActiveCount;

    FCL_GEOMETRY_REFERENCE Object1Ref;
    FCL_GEOMETRY_SNAPSHOT Object1Snapshot;
    FCL_GEOMETRY_REFERENCE Object2Ref;
    FCL_GEOMETRY_SNAPSHOT Object2Snapshot;

    struct {
        FCL_CONTACT_INFO Contact;
        FCL_COLLISION_RESULT Result;
    } Scratch;

    volatile LONG OverrunWarningIssued;
};

FCL_PERIODIC_COLLISION_STATE g_PeriodicCollisionState = {
    0,
    FALSE,
    {0, 0, 0},
    {},
    1,
    0,
    STATUS_SUCCESS,
    {},
    {},
    {},
    0,
    {},
    {},
    {},
    {},
    {},
    0
};

BOOLEAN
FclPeriodicCollisionSnapshotResult(
    _Out_ FCL_COLLISION_RESULT* result,
    _Out_ NTSTATUS* status,
    _Out_opt_ ULONGLONG* sequence) noexcept {
    if (result == nullptr || status == nullptr) {
        return FALSE;
    }

    for (int attempt = 0; attempt < 3; ++attempt) {
        const ULONGLONG first = g_PeriodicCollisionState.Sequence;
        KeMemoryBarrier();

        const FCL_COLLISION_RESULT localResult = g_PeriodicCollisionState.LastResult;
        const NTSTATUS localStatus = g_PeriodicCollisionState.LastStatus;

        KeMemoryBarrier();
        const ULONGLONG second = g_PeriodicCollisionState.Sequence;
        if (first == second) {
            *result = localResult;
            *status = localStatus;
            if (sequence != nullptr) {
                *sequence = second;
            }
            return TRUE;
        }
    }

    return FALSE;
}

_Function_class_(KDEFERRED_ROUTINE)
_IRQL_requires_(DISPATCH_LEVEL)
VOID
NTAPI
FclPeriodicCollisionDpc(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    ) noexcept;

struct PeriodicCollisionStateInitializer {
    PeriodicCollisionStateInitializer() {
        ExInitializePushLock(&g_PeriodicCollisionState.Lock);
        KeInitializeTimerEx(&g_PeriodicCollisionState.Timer, NotificationTimer);
        KeInitializeDpc(&g_PeriodicCollisionState.TimerDpc, FclPeriodicCollisionDpc, &g_PeriodicCollisionState);
        KeInitializeEvent(&g_PeriodicCollisionState.DpcIdleEvent, NotificationEvent, TRUE);
        g_PeriodicCollisionState.DpcActiveCount = 0;
    }
};

static PeriodicCollisionStateInitializer g_PeriodicCollisionStateInitializer;
inline BOOLEAN FclIsDpcNoDebugCrtEnabled() noexcept {
#ifdef FCL_MUSA_DPC_NO_DEBUG_CRT
    return TRUE;
#else
    return FALSE;
#endif
}
_Function_class_(KDEFERRED_ROUTINE)
_IRQL_requires_(DISPATCH_LEVEL)
VOID
NTAPI
FclPeriodicCollisionDpc(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    ) noexcept {
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    auto* state = static_cast<FCL_PERIODIC_COLLISION_STATE*>(DeferredContext);
    if (state == nullptr) {
        return;
    }

    const ULONGLONG startUs = QueryTimeMicroseconds();

    LONG newCount = InterlockedIncrement(&state->DpcActiveCount);
    if (newCount == 1) {
        KeClearEvent(&state->DpcIdleEvent);
    }

    ULONG innerIterations = state->InnerIterations;
    if (innerIterations == 0) {
        innerIterations = 1;
    }

    const FCL_GEOMETRY_SNAPSHOT object1 = state->Object1Snapshot;
    const FCL_GEOMETRY_SNAPSHOT object2 = state->Object2Snapshot;
    const FCL_TRANSFORM transform1 = state->Config.Transform1;
    const FCL_TRANSFORM transform2 = state->Config.Transform2;

    FCL_CONTACT_INFO* scratchContact = &state->Scratch.Contact;
    FCL_COLLISION_RESULT* scratchResult = &state->Scratch.Result;

    for (ULONG i = 0; i < innerIterations; ++i) {
        BOOLEAN isColliding = FALSE;
        RtlZeroMemory(scratchContact, sizeof(*scratchContact));
        RtlZeroMemory(scratchResult, sizeof(*scratchResult));
        NTSTATUS status = FclCollisionCoreFromSnapshots(
            &object1,
            &transform1,
            &object2,
            &transform2,
            &isColliding,
            scratchContact);

        scratchResult->IsColliding = isColliding ? 1 : 0;
        if (isColliding) {
            scratchResult->Contact = *scratchContact;
        } else {
            RtlZeroMemory(&scratchResult->Contact, sizeof(scratchResult->Contact));
        }

        state->LastStatus = status;
        state->LastResult = *scratchResult;
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&state->Sequence));
    }

    const ULONGLONG endUs = QueryTimeMicroseconds();
    if (startUs != 0 && endUs != 0) {
        const ULONGLONG elapsed = (endUs >= startUs) ? (endUs - startUs) : 0;
        if (elapsed > kPeriodicDpcBudgetUs) {
            if (InterlockedCompareExchange(&state->OverrunWarningIssued, 1, 0) == 0) {
                FCL_LOG_WARN("Periodic collision DPC exceeded budget: %llu us (limit=%llu us)",
                             static_cast<unsigned long long>(elapsed),
                             static_cast<unsigned long long>(kPeriodicDpcBudgetUs));
            }
        }
    }

    newCount = InterlockedDecrement(&state->DpcActiveCount);
    if (newCount == 0) {
        KeSetEvent(&state->DpcIdleEvent, IO_NO_INCREMENT, FALSE);
    }
}

NTSTATUS HandleStartPeriodicCollisionDpc(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_PERIODIC_COLLISION_CONFIG) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_PERIODIC_COLLISION_CONFIG)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* config = reinterpret_cast<FCL_PERIODIC_COLLISION_CONFIG*>(irp->AssociatedIrp.SystemBuffer);

    if (!FclIsGeometryHandleValid(config->Object1) ||
        !FclIsGeometryHandleValid(config->Object2) ||
        config->PeriodMicroseconds == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!fclmusa::geom::IsValidTransform(config->Transform1) ||
        !fclmusa::geom::IsValidTransform(config->Transform2)) {
        return STATUS_INVALID_PARAMETER;
    }

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PeriodicCollisionState.Lock);
    if (g_PeriodicCollisionState.Enabled) {
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PeriodicCollisionState.Lock);
        return STATUS_DEVICE_BUSY;
    }

    g_PeriodicCollisionState.Config = *config;
    {
        ULONG configuredIterations = config->Reserved[0];
        if (configuredIterations == 0) {
            configuredIterations = 1;
        }
        if (configuredIterations > 1024) {
            configuredIterations = 1024;
        }
        g_PeriodicCollisionState.InnerIterations = configuredIterations;
    }
    g_PeriodicCollisionState.Sequence = 0;
    g_PeriodicCollisionState.LastStatus = STATUS_SUCCESS;
    RtlZeroMemory(&g_PeriodicCollisionState.LastResult, sizeof(g_PeriodicCollisionState.LastResult));
    RtlZeroMemory(&g_PeriodicCollisionState.Scratch, sizeof(g_PeriodicCollisionState.Scratch));
    g_PeriodicCollisionState.OverrunWarningIssued = 0;

    // 释放旧引用（如果有）
    if (g_PeriodicCollisionState.Object1Ref.HandleValue != 0) {
        FclReleaseGeometryReference(&g_PeriodicCollisionState.Object1Ref);
        RtlZeroMemory(&g_PeriodicCollisionState.Object1Snapshot, sizeof(g_PeriodicCollisionState.Object1Snapshot));
    }
    if (g_PeriodicCollisionState.Object2Ref.HandleValue != 0) {
        FclReleaseGeometryReference(&g_PeriodicCollisionState.Object2Ref);
        RtlZeroMemory(&g_PeriodicCollisionState.Object2Snapshot, sizeof(g_PeriodicCollisionState.Object2Snapshot));
    }
    RtlZeroMemory(&g_PeriodicCollisionState.Scratch, sizeof(g_PeriodicCollisionState.Scratch));

    NTSTATUS status = FclAcquireGeometryReference(
        config->Object1,
        &g_PeriodicCollisionState.Object1Ref,
        &g_PeriodicCollisionState.Object1Snapshot);
    if (!NT_SUCCESS(status)) {
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PeriodicCollisionState.Lock);
        return status;
    }

    status = FclAcquireGeometryReference(
        config->Object2,
        &g_PeriodicCollisionState.Object2Ref,
        &g_PeriodicCollisionState.Object2Snapshot);
    if (!NT_SUCCESS(status)) {
        FclReleaseGeometryReference(&g_PeriodicCollisionState.Object1Ref);
        g_PeriodicCollisionState.Object1Ref.HandleValue = 0;
        RtlZeroMemory(&g_PeriodicCollisionState.Object1Snapshot, sizeof(g_PeriodicCollisionState.Object1Snapshot));
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PeriodicCollisionState.Lock);
        return status;
    }

    g_PeriodicCollisionState.Enabled = TRUE;
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PeriodicCollisionState.Lock);

    // 启动周期性 DPC 计时器
    g_PeriodicCollisionState.DpcActiveCount = 0;
    FCL_LOG_INFO("Periodic collision start (DPC) no-CRT=%d", FclIsDpcNoDebugCrtEnabled() ? 1 : 0);

    const ULONG periodMs = (config->PeriodMicroseconds + 999) / 1000;
    const LONG timerPeriodMs = (periodMs == 0) ? 1 : static_cast<LONG>(periodMs);

    LARGE_INTEGER dueTime = {};
    dueTime.QuadPart = -10LL * static_cast<LONGLONG>(timerPeriodMs) * 1000LL;

    KeSetTimerEx(
        &g_PeriodicCollisionState.Timer,
        dueTime,
        timerPeriodMs,
        &g_PeriodicCollisionState.TimerDpc);

    irp->IoStatus.Information = sizeof(FCL_PERIODIC_COLLISION_CONFIG);
    return status;
}

NTSTATUS HandleStopPeriodicCollisionDpc(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    UNREFERENCED_PARAMETER(stack);

    // 停止计时器并等待所有在途 DPC 完成
    KeCancelTimer(&g_PeriodicCollisionState.Timer);
    KeWaitForSingleObject(
        &g_PeriodicCollisionState.DpcIdleEvent,
        Executive,
        KernelMode,
        FALSE,
        nullptr);

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PeriodicCollisionState.Lock);

    g_PeriodicCollisionState.Enabled = FALSE;

    FCL_GEOMETRY_REFERENCE object1Ref = g_PeriodicCollisionState.Object1Ref;
    FCL_GEOMETRY_REFERENCE object2Ref = g_PeriodicCollisionState.Object2Ref;
    g_PeriodicCollisionState.Object1Ref.HandleValue = 0;
    g_PeriodicCollisionState.Object2Ref.HandleValue = 0;
    RtlZeroMemory(&g_PeriodicCollisionState.Object1Snapshot, sizeof(g_PeriodicCollisionState.Object1Snapshot));
    RtlZeroMemory(&g_PeriodicCollisionState.Object2Snapshot, sizeof(g_PeriodicCollisionState.Object2Snapshot));

    RtlZeroMemory(&g_PeriodicCollisionState.Config, sizeof(g_PeriodicCollisionState.Config));
    g_PeriodicCollisionState.Sequence = 0;
    g_PeriodicCollisionState.LastStatus = STATUS_SUCCESS;
    RtlZeroMemory(&g_PeriodicCollisionState.LastResult, sizeof(g_PeriodicCollisionState.LastResult));
    RtlZeroMemory(&g_PeriodicCollisionState.Scratch, sizeof(g_PeriodicCollisionState.Scratch));
    g_PeriodicCollisionState.OverrunWarningIssued = 0;

    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PeriodicCollisionState.Lock);

    if (object1Ref.HandleValue != 0) {
        FclReleaseGeometryReference(&object1Ref);
    }
    if (object2Ref.HandleValue != 0) {
        FclReleaseGeometryReference(&object2Ref);
    }

    FCL_COLLISION_RESULT lastResult = {};
    NTSTATUS lastStatus = STATUS_UNSUCCESSFUL;
    ULONGLONG lastSequence = 0;
    if (FclPeriodicCollisionSnapshotResult(&lastResult, &lastStatus, &lastSequence)) {
        FCL_LOG_INFO("Periodic collision final snapshot: seq=%llu status=0x%X colliding=%u",
                     static_cast<unsigned long long>(lastSequence),
                     lastStatus,
                     static_cast<unsigned>(lastResult.IsColliding));
    }

    FCL_LOG_INFO("Periodic collision stop (DPC) no-CRT=%d", FclIsDpcNoDebugCrtEnabled() ? 1 : 0);
    irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

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

NTSTATUS HandleSelfTestScenario(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_SELF_TEST_SCENARIO_REQUEST) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_SELF_TEST_SCENARIO_RESULT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* request = reinterpret_cast<FCL_SELF_TEST_SCENARIO_REQUEST*>(irp->AssociatedIrp.SystemBuffer);
    auto* result = reinterpret_cast<FCL_SELF_TEST_SCENARIO_RESULT*>(irp->AssociatedIrp.SystemBuffer);

    const FCL_SELF_TEST_SCENARIO_ID scenarioId = request->ScenarioId;
    RtlZeroMemory(result, sizeof(*result));

    NTSTATUS status = FclRunSelfTestScenario(scenarioId, result);
    if (NT_SUCCESS(status)) {
        irp->IoStatus.Information = sizeof(*result);
    }

    return status;
}

NTSTATUS HandleDiagnosticsQuery(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_DIAGNOSTICS_RESPONSE)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* response = reinterpret_cast<FCL_DIAGNOSTICS_RESPONSE*>(irp->AssociatedIrp.SystemBuffer);
    RtlZeroMemory(response, sizeof(*response));

    NTSTATUS status = FclQueryDiagnostics(response);
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

NTSTATUS HandleCreateMesh(_Inout_ PIRP irp, _In_ PIO_STACK_LOCATION stack) {
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(FCL_CREATE_MESH_BUFFER) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FCL_CREATE_MESH_BUFFER)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* buffer = reinterpret_cast<FCL_CREATE_MESH_BUFFER*>(irp->AssociatedIrp.SystemBuffer);
    if (buffer->VertexCount == 0 || buffer->IndexCount < 3 || (buffer->IndexCount % 3) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    size_t verticesSize = 0;
    NTSTATUS status = RtlSizeTMult(buffer->VertexCount, sizeof(FCL_VECTOR3), &verticesSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    size_t indicesSize = 0;
    status = RtlSizeTMult(buffer->IndexCount, sizeof(UINT32), &indicesSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    size_t requiredSize = 0;
    status = RtlSizeTAdd(sizeof(FCL_CREATE_MESH_BUFFER), verticesSize, &requiredSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = RtlSizeTAdd(requiredSize, indicesSize, &requiredSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (stack->Parameters.DeviceIoControl.InputBufferLength < requiredSize) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    auto* vertices = reinterpret_cast<FCL_VECTOR3*>(reinterpret_cast<BYTE*>(buffer) + sizeof(FCL_CREATE_MESH_BUFFER));
    auto* indices = reinterpret_cast<UINT32*>(reinterpret_cast<BYTE*>(vertices) + verticesSize);

    FCL_MESH_GEOMETRY_DESC desc = {};
    desc.Vertices = vertices;
    desc.VertexCount = buffer->VertexCount;
    desc.Indices = indices;
    desc.IndexCount = buffer->IndexCount;

    FCL_GEOMETRY_HANDLE handle = {};
    status = FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, &handle);
    if (NT_SUCCESS(status)) {
        buffer->Handle = handle;
        irp->IoStatus.Information = sizeof(FCL_CREATE_MESH_BUFFER);
    }
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
        case IOCTL_FCL_SELF_TEST_SCENARIO:
            status = HandleSelfTestScenario(irp, stack);
            break;
        case IOCTL_FCL_QUERY_DIAGNOSTICS:
            status = HandleDiagnosticsQuery(irp, stack);
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
        case IOCTL_FCL_CREATE_MESH:
            status = HandleCreateMesh(irp, stack);
            break;
        case IOCTL_FCL_CONVEX_CCD:
            status = HandleConvexCcdDemo(irp, stack);
            break;
        case IOCTL_FCL_START_PERIODIC_COLLISION:
            status = HandleStartPeriodicCollisionDpc(irp, stack);
            break;
        case IOCTL_FCL_STOP_PERIODIC_COLLISION:
            status = HandleStopPeriodicCollisionDpc(irp, stack);
            break;
#ifdef FCL_MUSA_ENABLE_DEMO
        case IOCTL_FCL_DEMO_SPHERE_COLLISION:
            status = HandleSphereCollisionDemo(irp, stack);
            break;
#endif
        default:
            FCL_LOG_WARN("Unsupported IOCTL: 0x%X", stack->Parameters.DeviceIoControl.IoControlCode);
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}
