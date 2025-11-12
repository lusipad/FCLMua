#pragma once

#include <ntddk.h>

#include "fclmusa/collision.h"
#include "fclmusa/geometry.h"

EXTERN_C_START

NTSTATUS
FclGjkIntersect(
    _In_ const FCL_GEOMETRY_SNAPSHOT* shapeA,
    _In_ const FCL_TRANSFORM* transformA,
    _In_ const FCL_GEOMETRY_SNAPSHOT* shapeB,
    _In_ const FCL_TRANSFORM* transformB,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

EXTERN_C_END
