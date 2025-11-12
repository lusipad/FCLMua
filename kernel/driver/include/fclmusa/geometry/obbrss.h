#pragma once

#include <ntddk.h>

#include "fclmusa/geometry/math_utils.h"

EXTERN_C_START

typedef struct _FCL_OBBRSS {
    FCL_VECTOR3 Center;
    FCL_VECTOR3 Axis[3];
    FCL_VECTOR3 Extents;
    float Radius;
} FCL_OBBRSS, *PFCL_OBBRSS;

FCL_OBBRSS
FclObbrssFromPoints(
    _In_reads_(pointCount) const FCL_VECTOR3* points,
    _In_ size_t pointCount) noexcept;

FCL_OBBRSS
FclObbrssMerge(
    _In_ const FCL_OBBRSS* lhs,
    _In_ const FCL_OBBRSS* rhs) noexcept;

BOOLEAN
FclObbrssOverlap(
    _In_ const FCL_OBBRSS* lhs,
    _In_ const FCL_OBBRSS* rhs) noexcept;

EXTERN_C_END
