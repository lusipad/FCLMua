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
