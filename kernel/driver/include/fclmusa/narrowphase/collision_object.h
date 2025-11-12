#pragma once

#include <ntddk.h>

#include "fclmusa/geometry.h"

typedef struct _FCL_COLLISION_OBJECT {
    FCL_GEOMETRY_HANDLE Geometry;
    FCL_TRANSFORM Transform;
} FCL_COLLISION_OBJECT, *PFCL_COLLISION_OBJECT;

typedef struct _FCL_COLLISION_REQUEST {
    ULONG MaxContacts;
    BOOLEAN EnableContactInfo;
} FCL_COLLISION_REQUEST, *PFCL_COLLISION_REQUEST;

typedef struct _FCL_COLLISION_RESULT {
    BOOLEAN Intersecting;
    ULONG ContactCount;
    FCL_CONTACT_INFO Contact;
} FCL_COLLISION_RESULT, *PFCL_COLLISION_RESULT;
