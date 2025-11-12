#include <ntddk.h>
#include <wdm.h>

#include "fclmusa/driver.h"
#include "fclmusa/logging.h"
#include "fclmusa/math/self_test.h"
#include "fclmusa/collision/self_test.h"
#include "fclmusa/geometry.h"
#include "fclmusa/memory/pool_allocator.h"
#include "fclmusa/runtime/musa_runtime_adapter.h"

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

NTSTATUS DemoTriangleMesh() {
    static const FCL_VECTOR3 vertices[] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    static const UINT32 indices[] = {
        0, 1, 2,
        0, 1, 3,
        0, 2, 3,
        1, 2, 3,
    };

    FCL_MESH_GEOMETRY_DESC desc = {};
    desc.Vertices = vertices;
    desc.VertexCount = RTL_NUMBER_OF(vertices);
    desc.Indices = indices;
    desc.IndexCount = RTL_NUMBER_OF(indices);

    FCL_GEOMETRY_HANDLE meshA = {};
    FCL_GEOMETRY_HANDLE meshB = {};
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, &meshA);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, &meshB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(meshA);
        return status;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation = {0.2f, 0.2f, 0.2f};

    BOOLEAN intersecting = FALSE;
    status = FclCollisionDetect(meshA, &transformA, meshB, &transformB, &intersecting, nullptr);

    FclDestroyGeometry(meshB);
    FclDestroyGeometry(meshA);
    return status;
}

NTSTATUS DemoSphereCollisionSimple() {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = 0.5f;

    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &sphereA);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    desc.Center.X = 1.0f;
    status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &sphereB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphereA);
        return status;
    }

    FCL_COLLISION_OBJECT_DESC objA = {sphereA, IdentityTransform()};
    FCL_COLLISION_OBJECT_DESC objB = {sphereB, IdentityTransform()};
    objB.Transform.Translation.X = 0.6f;

    FCL_COLLISION_QUERY_RESULT result = {};
    status = FclCollideObjects(&objA, &objB, nullptr, &result);

    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return status;
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

    status = FclRunMusaRuntimeSmokeTests();
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Musa.Runtime smoke test failed: 0x%X", status);
        goto Cleanup;
    }

    status = FclRunEigenSmokeTest();
    if (status == STATUS_NOT_SUPPORTED) {
        FCL_LOG_WARN0("Eigen headers not found; skipping math self-test");
        status = STATUS_SUCCESS;
    } else if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Eigen self-test failed: 0x%X", status);
        goto Cleanup;
    }

    status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Geometry subsystem initialization failed: 0x%X", status);
        goto Cleanup;
    }
    geometryInitialized = TRUE;


    status = FclRunCollisionSmokeTests();
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Collision smoke test failed: 0x%X", status);
        goto Cleanup;
    }

    if (NT_SUCCESS(status)) {
        status = DemoTriangleMesh();
        if (!NT_SUCCESS(status)) {
            FCL_LOG_ERROR("Triangle mesh demo failed: 0x%X", status);
            goto Cleanup;
        }
        status = DemoSphereCollisionSimple();
        if (!NT_SUCCESS(status)) {
            FCL_LOG_ERROR("Sphere collision demo failed: 0x%X", status);
            goto Cleanup;
        }
    }

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
