#pragma once

#include "fclmusa/platform.h"

#include <cstdint>

#include "fclmusa/version.h"

EXTERN_C_START

struct FCL_BVH_MODEL;

typedef struct _FCL_VECTOR3 {
    float X;
    float Y;
    float Z;
} FCL_VECTOR3, *PFCL_VECTOR3;

typedef struct _FCL_MATRIX3X3 {
    float M[3][3];
} FCL_MATRIX3X3, *PFCL_MATRIX3X3;

typedef struct _FCL_GEOMETRY_HANDLE {
    ULONGLONG Value;
} FCL_GEOMETRY_HANDLE, *PFCL_GEOMETRY_HANDLE;

typedef enum _FCL_GEOMETRY_TYPE {
    FCL_GEOMETRY_SPHERE = 1,
    FCL_GEOMETRY_OBB = 2,
    FCL_GEOMETRY_MESH = 3,
} FCL_GEOMETRY_TYPE;

typedef struct _FCL_SPHERE_GEOMETRY_DESC {
    FCL_VECTOR3 Center;
    float Radius;
} FCL_SPHERE_GEOMETRY_DESC, *PFCL_SPHERE_GEOMETRY_DESC;

typedef struct _FCL_OBB_GEOMETRY_DESC {
    FCL_VECTOR3 Center;
    FCL_VECTOR3 Extents;
    FCL_MATRIX3X3 Rotation;
} FCL_OBB_GEOMETRY_DESC, *PFCL_OBB_GEOMETRY_DESC;

typedef struct _FCL_MESH_GEOMETRY_DESC {
    const FCL_VECTOR3* Vertices;
    ULONG VertexCount;
    const UINT32* Indices;
    ULONG IndexCount;
} FCL_MESH_GEOMETRY_DESC, *PFCL_MESH_GEOMETRY_DESC;

typedef struct _FCL_TRANSFORM {
    FCL_MATRIX3X3 Rotation;
    FCL_VECTOR3 Translation;
} FCL_TRANSFORM, *PFCL_TRANSFORM;

typedef struct _FCL_GEOMETRY_SNAPSHOT {
    FCL_GEOMETRY_TYPE Type;
    union {
        FCL_SPHERE_GEOMETRY_DESC Sphere;
        FCL_OBB_GEOMETRY_DESC Obb;
        struct {
            const FCL_VECTOR3* Vertices;
            ULONG VertexCount;
            const UINT32* Indices;
            ULONG IndexCount;
            const FCL_BVH_MODEL* Bvh;
        } Mesh;
    } Data;
} FCL_GEOMETRY_SNAPSHOT, *PFCL_GEOMETRY_SNAPSHOT;

typedef struct _FCL_GEOMETRY_REFERENCE {
    ULONGLONG HandleValue;
} FCL_GEOMETRY_REFERENCE, *PFCL_GEOMETRY_REFERENCE;

NTSTATUS
FclGeometrySubsystemInitialize() noexcept;

VOID
FclGeometrySubsystemShutdown() noexcept;

NTSTATUS
FclCreateGeometry(
    _In_ FCL_GEOMETRY_TYPE type,
    _In_ const VOID* geometryDesc,
    _Out_ PFCL_GEOMETRY_HANDLE handle) noexcept;

NTSTATUS
FclDestroyGeometry(
    _In_ FCL_GEOMETRY_HANDLE handle) noexcept;

NTSTATUS
FclUpdateMeshGeometry(
    _In_ FCL_GEOMETRY_HANDLE handle,
    _In_ const FCL_MESH_GEOMETRY_DESC* geometryDesc) noexcept;

BOOLEAN
FclIsGeometryHandleValid(
    _In_ FCL_GEOMETRY_HANDLE handle) noexcept;

NTSTATUS
FclAcquireGeometryReference(
    _In_ FCL_GEOMETRY_HANDLE handle,
    _Out_ PFCL_GEOMETRY_REFERENCE reference,
    _Out_opt_ PFCL_GEOMETRY_SNAPSHOT snapshot) noexcept;

VOID
FclReleaseGeometryReference(
    _Inout_opt_ PFCL_GEOMETRY_REFERENCE reference) noexcept;

EXTERN_C_END
