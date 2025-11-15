#pragma once

#include <ntddk.h>

#include "fclmusa/collision.h"
#include "fclmusa/geometry.h"
#include "fclmusa/memory/pool_allocator.h"
#include "fclmusa/version.h"

//
// IOCTL 编号分组约定
//  - 0x800 - 0x80F : 稳定诊断 / 查询接口
//  - 0x810 - 0x83F : 正式几何 / 碰撞 / CCD 接口
//  - 0x900 - 0x9FF : Demo / 示例专用接口（不保证向后兼容）
//
// 注意：
//  - 推荐用户态代码仅依赖“稳定 + 正式”接口；
//  - Demo 接口仅用于示例和快速验证，可随实现调整。
//

// 诊断 / 查询 IOCTL（稳定）
#define IOCTL_FCL_PING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_SELF_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_SELF_TEST_SCENARIO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
// R0 �ں˲��Խ�����ز�ͳ�Ƽ��
#define IOCTL_FCL_QUERY_DIAGNOSTICS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

// 正式几何 / 碰撞 / 距离 IOCTL
#define IOCTL_FCL_QUERY_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_DISTANCE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_SPHERE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_DESTROY_GEOMETRY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_MESH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x814, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CONVEX_CCD CTL_CODE(FILE_DEVICE_UNKNOWN, 0x815, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

//
// 周期性（DPC+PASSIVE）碰撞检测控制 IOCTL
//  - IOCTL_FCL_START_PERIODIC_COLLISION: 配置并启动周期碰撞检测
//  - IOCTL_FCL_STOP_PERIODIC_COLLISION : 停止周期调度（不销毁几何对象）
//
#define IOCTL_FCL_START_PERIODIC_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x820, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_STOP_PERIODIC_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x821, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

// Demo / 示例 IOCTL（不承诺兼容性）
#ifdef FCL_MUSA_ENABLE_DEMO
#define IOCTL_FCL_DEMO_SPHERE_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

// 兼容别名（旧 Demo 名称，逐步迁移到 IOCTL_FCL_DEMO_ 前缀）
#define IOCTL_FCL_SPHERE_COLLISION IOCTL_FCL_DEMO_SPHERE_COLLISION
#endif

typedef struct _FCL_PING_RESPONSE {
    FCL_DRIVER_VERSION Version;
    BOOLEAN IsInitialized;
    BOOLEAN IsInitializing;
    ULONG Reserved;
    NTSTATUS LastError;
    LARGE_INTEGER Uptime100ns;
    FCL_POOL_STATS Pool;
} FCL_PING_RESPONSE, *PFCL_PING_RESPONSE;

static_assert((sizeof(FCL_PING_RESPONSE) % sizeof(ULONG)) == 0, "Ping response must align to ULONG");

typedef struct _FCL_CONTACT_SUMMARY {
    FCL_VECTOR3 PointOnObject1;
    FCL_VECTOR3 PointOnObject2;
    FCL_VECTOR3 Normal;
    float PenetrationDepth;
} FCL_CONTACT_SUMMARY, *PFCL_CONTACT_SUMMARY;

typedef enum _FCL_SELF_TEST_SCENARIO_ID {
    FCL_SELF_TEST_SCENARIO_RUNTIME = 1,
    FCL_SELF_TEST_SCENARIO_SPHERE_COLLISION = 2,
    FCL_SELF_TEST_SCENARIO_BROADPHASE = 3,
    FCL_SELF_TEST_SCENARIO_MESH_COLLISION = 4,
    FCL_SELF_TEST_SCENARIO_CCD = 5,
} FCL_SELF_TEST_SCENARIO_ID;

typedef struct _FCL_SELF_TEST_SCENARIO_REQUEST {
    FCL_SELF_TEST_SCENARIO_ID ScenarioId;
    ULONG Reserved[3];
} FCL_SELF_TEST_SCENARIO_REQUEST, *PFCL_SELF_TEST_SCENARIO_REQUEST;

typedef struct _FCL_SELF_TEST_SCENARIO_RESULT {
    FCL_SELF_TEST_SCENARIO_ID ScenarioId;
    NTSTATUS Status;
    ULONG Step;
    ULONG Reserved[3];
    FCL_POOL_STATS PoolBefore;
    FCL_POOL_STATS PoolAfter;
    FCL_CONTACT_SUMMARY Contact;
} FCL_SELF_TEST_SCENARIO_RESULT, *PFCL_SELF_TEST_SCENARIO_RESULT;

static_assert((sizeof(FCL_SELF_TEST_SCENARIO_REQUEST) % sizeof(ULONG)) == 0, "Self test scenario request must align to ULONG");
static_assert((sizeof(FCL_SELF_TEST_SCENARIO_RESULT) % sizeof(ULONG)) == 0, "Self test scenario result must align to ULONG");

typedef struct _FCL_SELF_TEST_RESULT {
    FCL_DRIVER_VERSION Version;
    NTSTATUS InitializeStatus;
    NTSTATUS GeometryCreateStatus;
    NTSTATUS CollisionStatus;
    NTSTATUS DestroyStatus;
    NTSTATUS DistanceStatus;
    NTSTATUS BroadphaseStatus;
    NTSTATUS MeshGjkStatus;
    NTSTATUS SphereMeshStatus;
    NTSTATUS MeshBroadphaseStatus;
    NTSTATUS ContinuousCollisionStatus;
    NTSTATUS GeometryUpdateStatus;
    NTSTATUS SphereObbStatus;
    NTSTATUS MeshComplexStatus;
    NTSTATUS BoundaryStatus;
    NTSTATUS DriverVerifierStatus;
    BOOLEAN DriverVerifierActive;
    NTSTATUS LeakTestStatus;
    NTSTATUS StressStatus;
    NTSTATUS PerformanceStatus;
    ULONGLONG StressDurationMicroseconds;
    ULONGLONG PerformanceDurationMicroseconds;
    NTSTATUS OverallStatus;
    BOOLEAN Passed;
    BOOLEAN PoolBalanced;
    BOOLEAN CollisionDetected;
    BOOLEAN BoundaryPassed;
    USHORT Reserved;
    NTSTATUS InvalidGeometryStatus;
    NTSTATUS DestroyInvalidStatus;
    NTSTATUS CollisionInvalidStatus;
    ULONGLONG PoolBytesDelta;
    float DistanceValue;
    ULONG BroadphasePairCount;
    ULONG MeshBroadphasePairCount;
    FCL_POOL_STATS PoolBefore;
    FCL_POOL_STATS PoolAfter;
    FCL_CONTACT_SUMMARY Contact;
} FCL_SELF_TEST_RESULT, *PFCL_SELF_TEST_RESULT;

static_assert((sizeof(FCL_SELF_TEST_RESULT) % sizeof(ULONG)) == 0, "Self test response must align to ULONG");

typedef struct _FCL_DETECTION_TIMING_STATS {
    ULONGLONG CallCount;
    ULONGLONG TotalDurationMicroseconds;
    ULONGLONG MinDurationMicroseconds;
    ULONGLONG MaxDurationMicroseconds;
} FCL_DETECTION_TIMING_STATS, *PFCL_DETECTION_TIMING_STATS;

typedef struct _FCL_DIAGNOSTICS_RESPONSE {
    FCL_DETECTION_TIMING_STATS Collision;
    FCL_DETECTION_TIMING_STATS Distance;
    FCL_DETECTION_TIMING_STATS ContinuousCollision;
    FCL_DETECTION_TIMING_STATS DpcCollision;
} FCL_DIAGNOSTICS_RESPONSE, *PFCL_DIAGNOSTICS_RESPONSE;

static_assert((sizeof(FCL_DIAGNOSTICS_RESPONSE) % sizeof(ULONG)) == 0, "Diagnostics response must align to ULONG");

typedef struct _FCL_COLLISION_QUERY {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM Transform2;
} FCL_COLLISION_QUERY, *PFCL_COLLISION_QUERY;

typedef struct _FCL_COLLISION_RESULT {
    UCHAR IsColliding;
    UCHAR Reserved[3];
    FCL_CONTACT_INFO Contact;
} FCL_COLLISION_RESULT, *PFCL_COLLISION_RESULT;

typedef struct _FCL_COLLISION_IO_BUFFER {
    FCL_COLLISION_QUERY Query;
    FCL_COLLISION_RESULT Result;
} FCL_COLLISION_IO_BUFFER, *PFCL_COLLISION_IO_BUFFER;

typedef struct _FCL_DISTANCE_QUERY {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM Transform2;
} FCL_DISTANCE_QUERY, *PFCL_DISTANCE_QUERY;

typedef struct _FCL_DISTANCE_OUTPUT {
    float Distance;
    FCL_VECTOR3 ClosestPoint1;
    FCL_VECTOR3 ClosestPoint2;
} FCL_DISTANCE_OUTPUT, *PFCL_DISTANCE_OUTPUT;

typedef struct _FCL_DISTANCE_IO_BUFFER {
    FCL_DISTANCE_QUERY Query;
    FCL_DISTANCE_OUTPUT Result;
} FCL_DISTANCE_IO_BUFFER, *PFCL_DISTANCE_IO_BUFFER;

typedef struct _FCL_CREATE_SPHERE_INPUT {
    FCL_SPHERE_GEOMETRY_DESC Desc;
} FCL_CREATE_SPHERE_INPUT, *PFCL_CREATE_SPHERE_INPUT;

typedef struct _FCL_CREATE_SPHERE_OUTPUT {
    FCL_GEOMETRY_HANDLE Handle;
} FCL_CREATE_SPHERE_OUTPUT, *PFCL_CREATE_SPHERE_OUTPUT;

typedef struct _FCL_DESTROY_INPUT {
    FCL_GEOMETRY_HANDLE Handle;
} FCL_DESTROY_INPUT, *PFCL_DESTROY_INPUT;

typedef struct _FCL_SPHERE_COLLISION_BUFFER {
    FCL_SPHERE_GEOMETRY_DESC SphereA;
    FCL_SPHERE_GEOMETRY_DESC SphereB;
    FCL_COLLISION_RESULT Result;
} FCL_SPHERE_COLLISION_BUFFER, *PFCL_SPHERE_COLLISION_BUFFER;

typedef struct _FCL_CONVEX_CCD_BUFFER {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_INTERP_MOTION Motion1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_INTERP_MOTION Motion2;
    FCL_CONTINUOUS_COLLISION_RESULT Result;
} FCL_CONVEX_CCD_BUFFER, *PFCL_CONVEX_CCD_BUFFER;

//
// 周期性碰撞检测配置：两几何对象 + 变换 + 周期
//
typedef struct _FCL_PERIODIC_COLLISION_CONFIG {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM Transform2;
    ULONG PeriodMicroseconds;  // 周期（微秒），例如 5000=5ms, 1000=1ms
    ULONG Reserved[3];
} FCL_PERIODIC_COLLISION_CONFIG, *PFCL_PERIODIC_COLLISION_CONFIG;

typedef struct _FCL_CREATE_MESH_BUFFER {
    UINT32 VertexCount;
    UINT32 IndexCount;
    UINT32 Reserved0;
    UINT32 Reserved1;
    FCL_GEOMETRY_HANDLE Handle;
    // Followed by VertexCount FCL_VECTOR3 entries and IndexCount UINT32 entries.
} FCL_CREATE_MESH_BUFFER, *PFCL_CREATE_MESH_BUFFER;

static_assert((sizeof(FCL_COLLISION_IO_BUFFER) % sizeof(ULONG)) == 0, "Collision IO buffer must align to ULONG");
static_assert((sizeof(FCL_DISTANCE_IO_BUFFER) % sizeof(ULONG)) == 0, "Distance IO buffer must align to ULONG");
static_assert((sizeof(FCL_CREATE_MESH_BUFFER) % sizeof(ULONG)) == 0, "Mesh buffer must align to ULONG");
static_assert((sizeof(FCL_PERIODIC_COLLISION_CONFIG) % sizeof(ULONG)) == 0, "Periodic collision config must align to ULONG");
