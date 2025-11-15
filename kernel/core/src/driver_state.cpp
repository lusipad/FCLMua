#include <ntddk.h>
#include <wdm.h>

#include "fclmusa/driver.h"
#include "fclmusa/logging.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/memory/pool_allocator.h"

using fclmusa::geom::IdentityTransform;

// Compatibility helpers for PushLock APIs
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

namespace {

typedef struct _FCL_DRIVER_STATE {
    EX_PUSH_LOCK Lock;
    BOOLEAN Initialized;
    BOOLEAN Initializing;
    LARGE_INTEGER StartTime;
    NTSTATUS LastError;
} FCL_DRIVER_STATE;

FCL_DRIVER_STATE g_DriverState = {
    0,  // Lock - will be initialized with ExInitializePushLock
    FALSE,
    FALSE,
    {0},
    STATUS_SUCCESS,
};

// Static initialization helper
struct DriverStateInitializer {
    DriverStateInitializer() {
        ExInitializePushLock(&g_DriverState.Lock);
    }
};
static DriverStateInitializer g_Initializer;

const FCL_DRIVER_VERSION g_DriverVersion = {
    FCL_MUSA_DRIVER_VERSION_MAJOR,
    FCL_MUSA_DRIVER_VERSION_MINOR,
    FCL_MUSA_DRIVER_VERSION_PATCH,
    FCL_MUSA_DRIVER_VERSION_BUILD,
};

LARGE_INTEGER QuerySystemTime100ns() {
#if (NTDDI_VERSION >= NTDDI_WIN8)
    LARGE_INTEGER now;
    KeQuerySystemTimePrecise(&now);
    return now;
#else
    LARGE_INTEGER now;
    KeQuerySystemTime(&now);
    return now;
#endif
}

struct DetectionTimingAccumulator {
    volatile LONG64 CallCount;
    volatile LONG64 TotalDurationMicroseconds;
    volatile LONG64 MinDurationMicroseconds;
    volatile LONG64 MaxDurationMicroseconds;
};

DetectionTimingAccumulator g_CollisionTiming = {};
DetectionTimingAccumulator g_DistanceTiming = {};
DetectionTimingAccumulator g_ContinuousCollisionTiming = {};
DetectionTimingAccumulator g_DpcCollisionTiming = {};

void ResetDetectionTimingUnsafe() noexcept {
    g_CollisionTiming.CallCount = 0;
    g_CollisionTiming.TotalDurationMicroseconds = 0;
    g_CollisionTiming.MinDurationMicroseconds = 0;
    g_CollisionTiming.MaxDurationMicroseconds = 0;

    g_DistanceTiming.CallCount = 0;
    g_DistanceTiming.TotalDurationMicroseconds = 0;
    g_DistanceTiming.MinDurationMicroseconds = 0;
    g_DistanceTiming.MaxDurationMicroseconds = 0;

    g_ContinuousCollisionTiming.CallCount = 0;
    g_ContinuousCollisionTiming.TotalDurationMicroseconds = 0;
    g_ContinuousCollisionTiming.MinDurationMicroseconds = 0;
    g_ContinuousCollisionTiming.MaxDurationMicroseconds = 0;

    g_DpcCollisionTiming.CallCount = 0;
    g_DpcCollisionTiming.TotalDurationMicroseconds = 0;
    g_DpcCollisionTiming.MinDurationMicroseconds = 0;
    g_DpcCollisionTiming.MaxDurationMicroseconds = 0;
}

void RecordDuration(DetectionTimingAccumulator& acc, ULONGLONG durationMicroseconds) noexcept {
    if (durationMicroseconds == 0) {
        durationMicroseconds = 1;
    }

    InterlockedIncrement64(&acc.CallCount);
    InterlockedAdd64(&acc.TotalDurationMicroseconds, static_cast<LONGLONG>(durationMicroseconds));

    LONGLONG observedMin = acc.MinDurationMicroseconds;
    if (observedMin == 0 || durationMicroseconds < static_cast<ULONGLONG>(observedMin)) {
        for (;;) {
            if (observedMin != 0 && durationMicroseconds >= static_cast<ULONGLONG>(observedMin)) {
                break;
            }
            const LONGLONG exchanged = InterlockedCompareExchange64(
                &acc.MinDurationMicroseconds,
                static_cast<LONGLONG>(durationMicroseconds),
                observedMin);
            if (exchanged == observedMin) {
                break;
            }
            observedMin = exchanged;
        }
    }

    LONGLONG observedMax = acc.MaxDurationMicroseconds;
    while (durationMicroseconds > static_cast<ULONGLONG>(observedMax)) {
        const LONGLONG exchanged = InterlockedCompareExchange64(
            &acc.MaxDurationMicroseconds,
            static_cast<LONGLONG>(durationMicroseconds),
            observedMax);
        if (exchanged == observedMax) {
            break;
        }
        observedMax = exchanged;
    }
}

void SnapshotTiming(const DetectionTimingAccumulator& acc, PFCL_DETECTION_TIMING_STATS snapshot) noexcept {
    if (snapshot == nullptr) {
        return;
    }

    snapshot->CallCount = static_cast<ULONGLONG>(acc.CallCount);
    snapshot->TotalDurationMicroseconds = static_cast<ULONGLONG>(acc.TotalDurationMicroseconds);
    snapshot->MinDurationMicroseconds = static_cast<ULONGLONG>(acc.MinDurationMicroseconds);
    snapshot->MaxDurationMicroseconds = static_cast<ULONGLONG>(acc.MaxDurationMicroseconds);
}

}  // namespace

extern "C" const FCL_DRIVER_VERSION* FclGetDriverVersion() {
    return &g_DriverVersion;
}

extern "C"
NTSTATUS
FclInitialize() {
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN geometryInitialized = FALSE;

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_DriverState.Lock);
    if (g_DriverState.Initializing) {
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_DriverState.Lock);
        return STATUS_DEVICE_BUSY;
    }

    if (g_DriverState.Initialized) {
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_DriverState.Lock);
        return STATUS_ALREADY_INITIALIZED;
    }

    g_DriverState.Initializing = TRUE;
    g_DriverState.LastError = STATUS_SUCCESS;
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_DriverState.Lock);

    fclmusa::memory::InitializePoolTracking();
    ResetDetectionTimingUnsafe();

    status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Geometry subsystem initialization failed: 0x%X", status);
        goto Cleanup;
    }
    geometryInitialized = TRUE;

Cleanup:
    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_DriverState.Lock);
    g_DriverState.Initializing = FALSE;
    if (NT_SUCCESS(status)) {
        g_DriverState.Initialized = TRUE;
        g_DriverState.LastError = STATUS_SUCCESS;
        g_DriverState.StartTime = QuerySystemTime100ns();
    } else {
        g_DriverState.Initialized = FALSE;
        g_DriverState.LastError = status;
        if (geometryInitialized) {
            FclGeometrySubsystemShutdown();
        }
        fclmusa::memory::ShutdownPoolTracking();
    }
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_DriverState.Lock);

    return status;
}

extern "C"
VOID
FclCleanup() {
    FclGeometrySubsystemShutdown();
    fclmusa::memory::ShutdownPoolTracking();

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_DriverState.Lock);
    g_DriverState.Initialized = FALSE;
    g_DriverState.Initializing = FALSE;
    g_DriverState.StartTime.QuadPart = 0;
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_DriverState.Lock);

    FCL_LOG_INFO0("FclCleanup completed");
}

extern "C"
NTSTATUS
FclQueryHealth(_Out_ FCL_PING_RESPONSE* response) {
    if (response == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(response, sizeof(*response));

    ExEnterCriticalRegionAndAcquirePushLockShared(&g_DriverState.Lock);
    response->Version = g_DriverVersion;
    response->IsInitialized = g_DriverState.Initialized;
    response->IsInitializing = g_DriverState.Initializing;
    response->LastError = g_DriverState.LastError;
    if (g_DriverState.Initialized) {
        const auto now = QuerySystemTime100ns();
        response->Uptime100ns.QuadPart = now.QuadPart - g_DriverState.StartTime.QuadPart;
    } else {
        response->Uptime100ns.QuadPart = 0;
    }
    ExReleasePushLockSharedAndLeaveCriticalRegion(&g_DriverState.Lock);

    response->Pool = fclmusa::memory::QueryStats();
    return STATUS_SUCCESS;
}

extern "C"
VOID
FclDiagnosticsRecordCollisionDuration(_In_ ULONGLONG durationMicroseconds) noexcept {
    RecordDuration(g_CollisionTiming, durationMicroseconds);
}

extern "C"
VOID
FclDiagnosticsRecordDpcCollisionDuration(_In_ ULONGLONG durationMicroseconds) noexcept {
    RecordDuration(g_DpcCollisionTiming, durationMicroseconds);
}

extern "C"
VOID
FclDiagnosticsRecordDistanceDuration(_In_ ULONGLONG durationMicroseconds) noexcept {
    RecordDuration(g_DistanceTiming, durationMicroseconds);
}

extern "C"
VOID
FclDiagnosticsRecordContinuousCollisionDuration(_In_ ULONGLONG durationMicroseconds) noexcept {
    RecordDuration(g_ContinuousCollisionTiming, durationMicroseconds);
}

extern "C"
NTSTATUS
FclQueryDiagnostics(_Out_ FCL_DIAGNOSTICS_RESPONSE* response) noexcept {
    if (response == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(response, sizeof(*response));

    SnapshotTiming(g_CollisionTiming, &response->Collision);
    SnapshotTiming(g_DistanceTiming, &response->Distance);
    SnapshotTiming(g_ContinuousCollisionTiming, &response->ContinuousCollision);
    SnapshotTiming(g_DpcCollisionTiming, &response->DpcCollision);

    return STATUS_SUCCESS;
}
