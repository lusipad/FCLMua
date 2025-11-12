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

EXTERN_C_END
