#pragma once

#include "fclmusa/platform.h"

#include "fclmusa/geometry.h"
#include "fclmusa/geometry/obbrss.h"

EXTERN_C_START

typedef struct _FCL_BVH_NODE {
    FCL_OBBRSS Volume;
    ULONG LeftChild;
    ULONG RightChild;
    ULONG FirstTriangle;
    ULONG TriangleCount;
} FCL_BVH_NODE, *PFCL_BVH_NODE;

NTSTATUS
FclBuildBvhModel(
    _In_reads_(vertexCount) const FCL_VECTOR3* vertices,
    _In_ ULONG vertexCount,
    _In_reads_(indexCount) const UINT32* indices,
    _In_ ULONG indexCount,
    _Outptr_ FCL_BVH_MODEL** model) noexcept;

NTSTATUS
FclBvhUpdateModel(
    _Inout_ FCL_BVH_MODEL* model,
    _In_reads_(vertexCount) const FCL_VECTOR3* vertices,
    _In_ ULONG vertexCount,
    _In_reads_(indexCount) const UINT32* indices,
    _In_ ULONG indexCount) noexcept;

VOID
FclDestroyBvhModel(
    _In_opt_ FCL_BVH_MODEL* model) noexcept;

const FCL_BVH_NODE*
FclBvhGetNodes(
    _In_opt_ const FCL_BVH_MODEL* model,
    _Out_opt_ ULONG* nodeCount) noexcept;

const UINT32*
FclBvhGetTriangleOrder(
    _In_opt_ const FCL_BVH_MODEL* model,
    _Out_opt_ ULONG* triangleCount) noexcept;

EXTERN_C_END
