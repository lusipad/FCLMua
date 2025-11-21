#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <ntddk.h>

#include "fclmusa/collision.h"
#include "fclmusa/distance.h"

NTSTATUS
FclUpstreamCollide(
    _In_ const FCL_GEOMETRY_SNAPSHOT& object1,
    _In_ const FCL_TRANSFORM& transform1,
    _In_ const FCL_GEOMETRY_SNAPSHOT& object2,
    _In_ const FCL_TRANSFORM& transform2,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

NTSTATUS
FclUpstreamDistance(
    _In_ const FCL_GEOMETRY_SNAPSHOT& object1,
    _In_ const FCL_TRANSFORM& transform1,
    _In_ const FCL_GEOMETRY_SNAPSHOT& object2,
    _In_ const FCL_TRANSFORM& transform2,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept;

NTSTATUS
FclUpstreamContinuousCollision(
    _In_ const FCL_GEOMETRY_SNAPSHOT& object1,
    _In_ const FCL_INTERP_MOTION& motion1,
    _In_ const FCL_GEOMETRY_SNAPSHOT& object2,
    _In_ const FCL_INTERP_MOTION& motion2,
    double tolerance,
    ULONG maxIterations,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept;
