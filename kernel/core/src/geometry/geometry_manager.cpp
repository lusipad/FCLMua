#include <ntddk.h>
#include <ntintsafe.h>
#include <wdm.h>

#include <float.h>
#include <new>

#include "fclmusa/geometry.h"
#include "fclmusa/geometry/bvh_model.h"
#include "fclmusa/logging.h"
#include "fclmusa/memory/pool_allocator.h"

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

namespace {

struct SpherePayload {
    FCL_VECTOR3 Center;
    float Radius;
};

struct ObbPayload {
    FCL_VECTOR3 Center;
    FCL_VECTOR3 Extents;
    FCL_MATRIX3X3 Rotation;
};

struct MeshPayload {
    FCL_VECTOR3* Vertices;
    ULONG VertexCount;
    UINT32* Indices;
    ULONG IndexCount;
    FCL_BVH_MODEL* Bvh;
};

struct GeometryEntry {
    ULONGLONG HandleValue;
    FCL_GEOMETRY_TYPE Type;
    volatile LONG ActiveReferences;
    union {
        SpherePayload Sphere;
        ObbPayload Obb;
        MeshPayload Mesh;
    } Payload;
};

RTL_AVL_TABLE g_GeometryTable = {};
EX_PUSH_LOCK g_GeometryLock = 0;
BOOLEAN g_GeometryInitialized = FALSE;
volatile LONG64 g_NextGeometryHandle = 0;

bool IsFiniteFloat(float value) noexcept {
    return (value == value) && (value < FLT_MAX) && (value > -FLT_MAX);
}

bool IsValidVector(const FCL_VECTOR3& vector) noexcept {
    return IsFiniteFloat(vector.X) && IsFiniteFloat(vector.Y) && IsFiniteFloat(vector.Z);
}

bool IsValidMatrix(const FCL_MATRIX3X3& matrix) noexcept {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (!IsFiniteFloat(matrix.M[row][col])) {
                return false;
            }
        }
    }
    return true;
}

NTSTATUS ValidateSphereDesc(const FCL_SPHERE_GEOMETRY_DESC* desc) noexcept {
    if (desc == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    if (!IsValidVector(desc->Center)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (!IsFiniteFloat(desc->Radius) || desc->Radius <= 0.0f) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS ValidateObbDesc(const FCL_OBB_GEOMETRY_DESC* desc) noexcept {
    if (desc == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    if (!IsValidVector(desc->Center) || !IsValidVector(desc->Extents)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (!IsValidMatrix(desc->Rotation)) {
        return STATUS_INVALID_PARAMETER;
    }
    if (desc->Extents.X <= 0.0f || desc->Extents.Y <= 0.0f || desc->Extents.Z <= 0.0f) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS ValidateMeshDesc(const FCL_MESH_GEOMETRY_DESC* desc) noexcept {
    if (desc == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    if (desc->Vertices == nullptr || desc->Indices == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    if (desc->VertexCount == 0 || desc->IndexCount < 3 || (desc->IndexCount % 3) != 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

void ReleasePayload(GeometryEntry& entry) noexcept {
    if (entry.Type == FCL_GEOMETRY_MESH) {
        auto& mesh = entry.Payload.Mesh;
        if (mesh.Vertices != nullptr) {
            fclmusa::memory::Free(mesh.Vertices);
            mesh.Vertices = nullptr;
        }
        if (mesh.Indices != nullptr) {
            fclmusa::memory::Free(mesh.Indices);
            mesh.Indices = nullptr;
        }
        mesh.VertexCount = 0;
        mesh.IndexCount = 0;
        if (mesh.Bvh != nullptr) {
            FclDestroyBvhModel(mesh.Bvh);
            mesh.Bvh = nullptr;
        }
    }
}

NTSTATUS CopyMeshPayload(const FCL_MESH_GEOMETRY_DESC* desc, MeshPayload* payload) noexcept {
    if (payload == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    size_t verticesSize = 0;
    NTSTATUS status = RtlSizeTMult(desc->VertexCount, sizeof(FCL_VECTOR3), &verticesSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    size_t indicesSize = 0;
    status = RtlSizeTMult(desc->IndexCount, sizeof(UINT32), &indicesSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    auto* vertices = static_cast<FCL_VECTOR3*>(fclmusa::memory::Allocate(verticesSize));
    if (vertices == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    auto* indices = static_cast<UINT32*>(fclmusa::memory::Allocate(indicesSize));
    if (indices == nullptr) {
        fclmusa::memory::Free(vertices);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    __try {
        RtlCopyMemory(vertices, desc->Vertices, verticesSize);
        RtlCopyMemory(indices, desc->Indices, indicesSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fclmusa::memory::Free(vertices);
        fclmusa::memory::Free(indices);
        return GetExceptionCode();
    }

    FCL_VECTOR3* oldVertices = payload->Vertices;
    UINT32* oldIndices = payload->Indices;
    ULONG oldVertexCount = payload->VertexCount;
    ULONG oldIndexCount = payload->IndexCount;
    FCL_BVH_MODEL* existingModel = payload->Bvh;

    payload->Vertices = vertices;
    payload->VertexCount = desc->VertexCount;
    payload->Indices = indices;
    payload->IndexCount = desc->IndexCount;

    if (existingModel != nullptr) {
        status = FclBvhUpdateModel(
            existingModel,
            payload->Vertices,
            payload->VertexCount,
            payload->Indices,
            payload->IndexCount);
        payload->Bvh = existingModel;
    } else {
        status = FclBuildBvhModel(
            payload->Vertices,
            payload->VertexCount,
            payload->Indices,
            payload->IndexCount,
            &payload->Bvh);
    }

    if (!NT_SUCCESS(status)) {
        fclmusa::memory::Free(payload->Vertices);
        fclmusa::memory::Free(payload->Indices);
        payload->Vertices = oldVertices;
        payload->Indices = oldIndices;
        payload->VertexCount = oldVertexCount;
        payload->IndexCount = oldIndexCount;
        payload->Bvh = existingModel;
        return status;
    }

    if (oldVertices != nullptr) {
        fclmusa::memory::Free(oldVertices);
    }
    if (oldIndices != nullptr) {
        fclmusa::memory::Free(oldIndices);
    }
    return STATUS_SUCCESS;
}

GeometryEntry MakeEntryTemplate(FCL_GEOMETRY_TYPE type) noexcept {
    GeometryEntry entry = {};
    entry.HandleValue = 0;
    entry.Type = type;
    entry.ActiveReferences = 0;
    entry.Payload.Mesh.Vertices = nullptr;
    entry.Payload.Mesh.Indices = nullptr;
    entry.Payload.Mesh.VertexCount = 0;
    entry.Payload.Mesh.IndexCount = 0;
    entry.Payload.Mesh.Bvh = nullptr;
    return entry;
}

using GeometryCompareRoutine = RTL_AVL_COMPARE_ROUTINE;
using GeometryAllocateRoutine = RTL_AVL_ALLOCATE_ROUTINE;
using GeometryFreeRoutine = RTL_AVL_FREE_ROUTINE;

_Function_class_(PRTL_AVL_COMPARE_ROUTINE)
RTL_GENERIC_COMPARE_RESULTS
NTAPI
CompareEntries(
    _In_ PRTL_AVL_TABLE table,
    _In_ PVOID first,
    _In_ PVOID second) {
    UNREFERENCED_PARAMETER(table);
    auto lhs = reinterpret_cast<const GeometryEntry*>(first);
    auto rhs = reinterpret_cast<const GeometryEntry*>(second);
    if (lhs->HandleValue < rhs->HandleValue) {
        return GenericLessThan;
    }
    if (lhs->HandleValue > rhs->HandleValue) {
        return GenericGreaterThan;
    }
    return GenericEqual;
}

_Function_class_(PRTL_AVL_ALLOCATE_ROUTINE)
PVOID
NTAPI
AllocateEntry(
    _In_ PRTL_AVL_TABLE table,
    _In_ CLONG byteSize) {
    UNREFERENCED_PARAMETER(table);
    return fclmusa::memory::Allocate(static_cast<size_t>(byteSize));
}

_Function_class_(PRTL_AVL_FREE_ROUTINE)
VOID
NTAPI
FreeEntry(
    _In_ PRTL_AVL_TABLE table,
    _In_ __drv_freesMem(Mem) PVOID buffer) {
    UNREFERENCED_PARAMETER(table);
    fclmusa::memory::Free(buffer);
}

GeometryEntry* LookupEntryLocked(ULONGLONG handleValue) noexcept {
    GeometryEntry key = {};
    key.HandleValue = handleValue;
    return reinterpret_cast<GeometryEntry*>(
        RtlLookupElementGenericTableAvl(&g_GeometryTable, &key));
}

NTSTATUS InsertEntryLocked(GeometryEntry* entryTemplate, PFCL_GEOMETRY_HANDLE handle) noexcept {
    const ULONGLONG handleValue = static_cast<ULONGLONG>(InterlockedIncrement64(&g_NextGeometryHandle));
    entryTemplate->HandleValue = handleValue;

    BOOLEAN newElement = FALSE;
    auto* inserted = reinterpret_cast<GeometryEntry*>(
        RtlInsertElementGenericTableAvl(&g_GeometryTable, entryTemplate, sizeof(*entryTemplate), &newElement));

    if (!newElement || inserted == nullptr) {
        return STATUS_INTERNAL_ERROR;
    }

    handle->Value = handleValue;
    return STATUS_SUCCESS;
}

bool EnsureInitialized() noexcept {
    return g_GeometryInitialized != FALSE;
}

void CopyEntryToSnapshot(const GeometryEntry& entry, PFCL_GEOMETRY_SNAPSHOT snapshot) noexcept {
    if (snapshot == nullptr) {
        return;
    }

    snapshot->Type = entry.Type;
    switch (entry.Type) {
        case FCL_GEOMETRY_SPHERE:
            snapshot->Data.Sphere.Center = entry.Payload.Sphere.Center;
            snapshot->Data.Sphere.Radius = entry.Payload.Sphere.Radius;
            break;
        case FCL_GEOMETRY_OBB:
            snapshot->Data.Obb.Center = entry.Payload.Obb.Center;
            snapshot->Data.Obb.Extents = entry.Payload.Obb.Extents;
            snapshot->Data.Obb.Rotation = entry.Payload.Obb.Rotation;
            break;
        case FCL_GEOMETRY_MESH:
            snapshot->Data.Mesh.Vertices = entry.Payload.Mesh.Vertices;
            snapshot->Data.Mesh.VertexCount = entry.Payload.Mesh.VertexCount;
            snapshot->Data.Mesh.Indices = entry.Payload.Mesh.Indices;
            snapshot->Data.Mesh.IndexCount = entry.Payload.Mesh.IndexCount;
            snapshot->Data.Mesh.Bvh = entry.Payload.Mesh.Bvh;
            break;
        default:
            break;
    }
}

}  // namespace

extern "C"
NTSTATUS
FclGeometrySubsystemInitialize() noexcept {
    if (g_GeometryInitialized) {
        return STATUS_SUCCESS;
    }

    ExInitializePushLock(&g_GeometryLock);
    RtlInitializeGenericTableAvl(
        &g_GeometryTable,
        CompareEntries,
        AllocateEntry,
        FreeEntry,
        nullptr);
    g_NextGeometryHandle = 0;
    g_GeometryInitialized = TRUE;

    return STATUS_SUCCESS;
}

extern "C"
VOID
FclGeometrySubsystemShutdown() noexcept {
    if (!g_GeometryInitialized) {
        return;
    }

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_GeometryLock);

    while (!RtlIsGenericTableEmptyAvl(&g_GeometryTable)) {
        auto* entry = reinterpret_cast<GeometryEntry*>(RtlGetElementGenericTableAvl(&g_GeometryTable, 0));
        if (entry == nullptr) {
            break;
        }
        GeometryEntry temp = *entry;
        RtlDeleteElementGenericTableAvl(&g_GeometryTable, entry);
        ReleasePayload(temp);
    }

    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);
    g_GeometryInitialized = FALSE;
}

extern "C"
NTSTATUS
FclCreateGeometry(
    _In_ FCL_GEOMETRY_TYPE type,
    _In_ const VOID* geometryDesc,
    _Out_ PFCL_GEOMETRY_HANDLE handle) noexcept {
    if (handle == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    handle->Value = 0;

    if (!EnsureInitialized()) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (geometryDesc == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_SUCCESS;
    GeometryEntry entry = MakeEntryTemplate(type);

    switch (type) {
        case FCL_GEOMETRY_SPHERE: {
            auto* desc = reinterpret_cast<const FCL_SPHERE_GEOMETRY_DESC*>(geometryDesc);
            status = ValidateSphereDesc(desc);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            entry.Payload.Sphere.Center = desc->Center;
            entry.Payload.Sphere.Radius = desc->Radius;
            break;
        }
        case FCL_GEOMETRY_OBB: {
            auto* desc = reinterpret_cast<const FCL_OBB_GEOMETRY_DESC*>(geometryDesc);
            status = ValidateObbDesc(desc);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            entry.Payload.Obb.Center = desc->Center;
            entry.Payload.Obb.Extents = desc->Extents;
            entry.Payload.Obb.Rotation = desc->Rotation;
            break;
        }
        case FCL_GEOMETRY_MESH: {
            auto* desc = reinterpret_cast<const FCL_MESH_GEOMETRY_DESC*>(geometryDesc);
            status = ValidateMeshDesc(desc);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            status = CopyMeshPayload(desc, &entry.Payload.Mesh);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            break;
        }
        default:
            return STATUS_INVALID_PARAMETER;
    }

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_GeometryLock);
    status = InsertEntryLocked(&entry, handle);
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);

    if (!NT_SUCCESS(status)) {
        ReleasePayload(entry);
    }

    return status;
}

extern "C"
NTSTATUS
FclDestroyGeometry(
    _In_ FCL_GEOMETRY_HANDLE handleValue) noexcept {
    if (!EnsureInitialized()) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (!FclIsGeometryHandleValid(handleValue)) {
        return STATUS_INVALID_HANDLE;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_GeometryLock);

    auto* entry = LookupEntryLocked(handleValue.Value);
    if (entry == nullptr) {
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);
        return STATUS_INVALID_HANDLE;
    }

    if (entry->ActiveReferences != 0) {
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);
        return STATUS_DEVICE_BUSY;
    }

    GeometryEntry temp = *entry;
    const BOOLEAN removed = RtlDeleteElementGenericTableAvl(&g_GeometryTable, entry);

    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);

    if (!removed) {
        return STATUS_INTERNAL_ERROR;
    }

    ReleasePayload(temp);
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclUpdateMeshGeometry(
    _In_ FCL_GEOMETRY_HANDLE handleValue,
    _In_ const FCL_MESH_GEOMETRY_DESC* geometryDesc) noexcept {
    if (!EnsureInitialized()) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (!FclIsGeometryHandleValid(handleValue)) {
        return STATUS_INVALID_HANDLE;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    NTSTATUS status = ValidateMeshDesc(geometryDesc);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_GeometryLock);

    GeometryEntry* entry = LookupEntryLocked(handleValue.Value);
    if (entry == nullptr) {
        status = STATUS_INVALID_HANDLE;
        goto Exit;
    }

    if (entry->Type != FCL_GEOMETRY_MESH) {
        status = STATUS_NOT_SUPPORTED;
        goto Exit;
    }

    if (entry->ActiveReferences != 0) {
        status = STATUS_DEVICE_BUSY;
        goto Exit;
    }

    status = CopyMeshPayload(geometryDesc, &entry->Payload.Mesh);

Exit:
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);
    return status;
}

extern "C"
BOOLEAN
FclIsGeometryHandleValid(
    _In_ FCL_GEOMETRY_HANDLE handle) noexcept {
    return handle.Value != 0;
}

extern "C"
NTSTATUS
FclAcquireGeometryReference(
    _In_ FCL_GEOMETRY_HANDLE handle,
    _Out_ PFCL_GEOMETRY_REFERENCE reference,
    _Out_opt_ PFCL_GEOMETRY_SNAPSHOT snapshot) noexcept {
    if (reference == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    reference->HandleValue = 0;

    if (!EnsureInitialized()) {
        return STATUS_DEVICE_NOT_READY;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (!FclIsGeometryHandleValid(handle)) {
        return STATUS_INVALID_HANDLE;
    }

    NTSTATUS status = STATUS_SUCCESS;

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_GeometryLock);
    auto* entry = LookupEntryLocked(handle.Value);
    if (entry == nullptr) {
        status = STATUS_INVALID_HANDLE;
    } else {
        ++entry->ActiveReferences;
        CopyEntryToSnapshot(*entry, snapshot);
        reference->HandleValue = handle.Value;
    }
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);

    if (!NT_SUCCESS(status) && snapshot != nullptr) {
        RtlZeroMemory(snapshot, sizeof(*snapshot));
    }

    return status;
}

extern "C"
VOID
FclReleaseGeometryReference(
    _Inout_opt_ PFCL_GEOMETRY_REFERENCE reference) noexcept {
    if (reference == nullptr || reference->HandleValue == 0) {
        return;
    }

    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_GeometryLock);
    auto* entry = LookupEntryLocked(reference->HandleValue);
    if (entry != nullptr && entry->ActiveReferences > 0) {
        --entry->ActiveReferences;
    }
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_GeometryLock);

    reference->HandleValue = 0;
}
