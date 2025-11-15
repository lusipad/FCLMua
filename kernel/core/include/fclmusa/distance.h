#pragma once

#include <ntddk.h>

#include "fclmusa/geometry.h"

EXTERN_C_START

typedef struct _FCL_DISTANCE_RESULT {
    float Distance;
    FCL_VECTOR3 ClosestPoint1;
    FCL_VECTOR3 ClosestPoint2;
} FCL_DISTANCE_RESULT, *PFCL_DISTANCE_RESULT;

NTSTATUS
FclDistanceCompute(
    _In_ FCL_GEOMETRY_HANDLE object1,
    _In_opt_ const FCL_TRANSFORM* transform1,
    _In_ FCL_GEOMETRY_HANDLE object2,
    _In_opt_ const FCL_TRANSFORM* transform2,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept;

//
// 内部 Snapshot Core API（IRQL <= DISPATCH_LEVEL，可在 DPC 中调用）
// - 仅使用几何快照 / 变换，不执行句柄查找或加锁
//
NTSTATUS
FclDistanceCoreFromSnapshots(
    _In_ const FCL_GEOMETRY_SNAPSHOT* object1,
    _In_ const FCL_TRANSFORM* transform1,
    _In_ const FCL_GEOMETRY_SNAPSHOT* object2,
    _In_ const FCL_TRANSFORM* transform2,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept;

EXTERN_C_END
