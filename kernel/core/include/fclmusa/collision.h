#pragma once

#include <ntddk.h>

#include "fclmusa/geometry.h"

EXTERN_C_START

typedef struct _FCL_CONTACT_INFO {
    FCL_VECTOR3 PointOnObject1;
    FCL_VECTOR3 PointOnObject2;
    FCL_VECTOR3 Normal;
    float PenetrationDepth;
} FCL_CONTACT_INFO, *PFCL_CONTACT_INFO;

typedef struct _FCL_COLLISION_OBJECT_DESC {
    FCL_GEOMETRY_HANDLE Geometry;
    FCL_TRANSFORM Transform;
} FCL_COLLISION_OBJECT_DESC, *PFCL_COLLISION_OBJECT_DESC;

typedef struct _FCL_COLLISION_QUERY_REQUEST {
    ULONG MaxContacts;
    BOOLEAN EnableContactInfo;
} FCL_COLLISION_QUERY_REQUEST, *PFCL_COLLISION_QUERY_REQUEST;

typedef struct _FCL_COLLISION_QUERY_RESULT {
    BOOLEAN Intersecting;
    ULONG ContactCount;
    FCL_CONTACT_INFO Contact;
} FCL_COLLISION_QUERY_RESULT, *PFCL_COLLISION_QUERY_RESULT;

typedef struct _FCL_INTERP_MOTION_DESC {
    FCL_TRANSFORM Start;
    FCL_TRANSFORM End;
} FCL_INTERP_MOTION_DESC, *PFCL_INTERP_MOTION_DESC;

typedef struct _FCL_INTERP_MOTION {
    FCL_TRANSFORM Start;
    FCL_TRANSFORM End;
} FCL_INTERP_MOTION, *PFCL_INTERP_MOTION;

typedef struct _FCL_SCREW_MOTION_DESC {
    FCL_TRANSFORM Start;
    FCL_TRANSFORM End;
} FCL_SCREW_MOTION_DESC, *PFCL_SCREW_MOTION_DESC;

typedef struct _FCL_SCREW_MOTION {
    FCL_TRANSFORM Start;
    FCL_VECTOR3 Axis;
    float AngularVelocity;
    float LinearVelocity;
    FCL_VECTOR3 OrthogonalTranslation;
} FCL_SCREW_MOTION, *PFCL_SCREW_MOTION;

typedef struct _FCL_CONTINUOUS_COLLISION_QUERY {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_INTERP_MOTION Motion1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_INTERP_MOTION Motion2;
    double Tolerance;
    ULONG MaxIterations;
} FCL_CONTINUOUS_COLLISION_QUERY, *PFCL_CONTINUOUS_COLLISION_QUERY;

typedef struct _FCL_CONTINUOUS_COLLISION_RESULT {
    BOOLEAN Intersecting;
    double TimeOfImpact;
    FCL_CONTACT_INFO Contact;
} FCL_CONTINUOUS_COLLISION_RESULT, *PFCL_CONTINUOUS_COLLISION_RESULT;

NTSTATUS
FclCollisionDetect(
    _In_ FCL_GEOMETRY_HANDLE object1,
    _In_opt_ const FCL_TRANSFORM* transform1,
    _In_ FCL_GEOMETRY_HANDLE object2,
    _In_opt_ const FCL_TRANSFORM* transform2,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

NTSTATUS
FclCollideObjects(
    _In_ const FCL_COLLISION_OBJECT_DESC* object1,
    _In_ const FCL_COLLISION_OBJECT_DESC* object2,
    _In_opt_ const FCL_COLLISION_QUERY_REQUEST* request,
    _Out_ PFCL_COLLISION_QUERY_RESULT result) noexcept;

NTSTATUS
FclInterpMotionInitialize(
    _In_ const FCL_INTERP_MOTION_DESC* desc,
    _Out_ PFCL_INTERP_MOTION motion) noexcept;

NTSTATUS
FclInterpMotionEvaluate(
    _In_ const FCL_INTERP_MOTION* motion,
    _In_ double t,
    _Out_ PFCL_TRANSFORM transform) noexcept;

NTSTATUS
FclScrewMotionInitialize(
    _In_ const FCL_SCREW_MOTION_DESC* desc,
    _Out_ PFCL_SCREW_MOTION motion) noexcept;

NTSTATUS
FclScrewMotionEvaluate(
    _In_ const FCL_SCREW_MOTION* motion,
    _In_ double t,
    _Out_ PFCL_TRANSFORM transform) noexcept;

NTSTATUS
FclContinuousCollision(
    _In_ const FCL_CONTINUOUS_COLLISION_QUERY* query,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept;

//
// 内部 Snapshot Core API（IRQL <= DISPATCH_LEVEL，可在 DPC 中调用）
// - 仅使用几何快照 / 变换 / 运动描述，不执行句柄查找或加锁
// - 外部调用方负责确保所有指针指向 NonPagedPool 中的有效数据
//
NTSTATUS
FclCollisionCoreFromSnapshots(
    _In_ const FCL_GEOMETRY_SNAPSHOT* object1,
    _In_ const FCL_TRANSFORM* transform1,
    _In_ const FCL_GEOMETRY_SNAPSHOT* object2,
    _In_ const FCL_TRANSFORM* transform2,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept;

NTSTATUS
FclContinuousCollisionCoreFromSnapshots(
    _In_ const FCL_GEOMETRY_SNAPSHOT* object1,
    _In_ const FCL_INTERP_MOTION* motion1,
    _In_ const FCL_GEOMETRY_SNAPSHOT* object2,
    _In_ const FCL_INTERP_MOTION* motion2,
    _In_ double tolerance,
    _In_ ULONG maxIterations,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept;

EXTERN_C_END
