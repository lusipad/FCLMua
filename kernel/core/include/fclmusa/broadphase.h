#pragma once

#include <ntddk.h>

#include "fclmusa/geometry.h"

EXTERN_C_START

typedef struct _FCL_BROADPHASE_OBJECT {
    FCL_GEOMETRY_HANDLE Handle;
    const FCL_TRANSFORM* Transform;
} FCL_BROADPHASE_OBJECT, *PFCL_BROADPHASE_OBJECT;

typedef struct _FCL_BROADPHASE_PAIR {
    FCL_GEOMETRY_HANDLE A;
    FCL_GEOMETRY_HANDLE B;
} FCL_BROADPHASE_PAIR, *PFCL_BROADPHASE_PAIR;

NTSTATUS
FclBroadphaseDetect(
    _In_reads_(objectCount) const FCL_BROADPHASE_OBJECT* objects,
    _In_ ULONG objectCount,
    _Out_writes_opt_(pairCapacity) PFCL_BROADPHASE_PAIR pairs,
    _In_ ULONG pairCapacity,
    _Out_ PULONG pairCount) noexcept;

EXTERN_C_END
