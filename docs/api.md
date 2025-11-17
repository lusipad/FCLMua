# FCL+Musa Driver API 文档

本文档详细说明 FCL 内核管理模块暴露的 C 接口。IOCTL 层只是将用户态 buffer 映射到这些 API，并不包含额外业务逻辑。

---

## 初始化与清理

### NTSTATUS FclInitialize()
**功能**: 初始化内部内存系统、Musa.Runtime、几何表和碰撞模块。

**返回值**:
- `STATUS_SUCCESS` - 初始化成功
- `STATUS_ALREADY_INITIALIZED` - 已经初始化，重复调用会返回此错误

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 必须在 `DriverEntry` 中调用，确保所有子系统正确初始化后才能使用其他 API。

---

### VOID FclCleanup()
**功能**: 释放内部资源、停止内存统计。

**返回值**: 无

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 在驱动卸载或进程终止时调用，确保所有几何对象已被销毁。

---

## 几何管理

### NTSTATUS FclCreateGeometry(FCL_GEOMETRY_TYPE type, const VOID* geometryDesc, FCL_GEOMETRY_HANDLE* handle)
**功能**: 创建 Sphere / OBB / Mesh 几何对象。

**参数**:
- `type` - 几何类型：
  - `FCL_GEOMETRY_SPHERE` (1) - 球体
  - `FCL_GEOMETRY_OBB` (2) - 有向包围盒
  - `FCL_GEOMETRY_MESH` (3) - 三角网格
- `geometryDesc` - 几何描述结构指针：
  - Sphere: `FCL_SPHERE_GEOMETRY_DESC*` (Center, Radius)
  - OBB: `FCL_OBB_GEOMETRY_DESC*` (Center, Extents, Rotation)
  - Mesh: `FCL_MESH_GEOMETRY_DESC*` (Vertices, VertexCount, Indices, IndexCount)
- `handle` - 输出参数，返回有效的几何句柄

**返回值**:
- `STATUS_SUCCESS` - 创建成功
- `STATUS_INVALID_PARAMETER` - 参数非法（如空指针、无效类型）
- `STATUS_INSUFFICIENT_RESOURCES` - 内存分配失败

**IRQL要求**: `PASSIVE_LEVEL`

**说明**:
- Mesh 几何会自动构建 BVH 加速结构
- 句柄由几何管理器维护，使用 AVL 树索引
- 创建的几何对象使用 NonPagedPool 内存

---

### NTSTATUS FclDestroyGeometry(FCL_GEOMETRY_HANDLE handle)
**功能**: 销毁几何对象并释放资源。

**参数**:
- `handle` - 要销毁的几何句柄

**返回值**:
- `STATUS_SUCCESS` - 销毁成功
- `STATUS_INVALID_HANDLE` - 句柄无效或已被销毁
- `STATUS_DEVICE_BUSY` - 对象仍有未释放的引用

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 确保没有活动引用后才会真正释放，使用引用计数机制。

---

### NTSTATUS FclUpdateMeshGeometry(FCL_GEOMETRY_HANDLE handle, const FCL_MESH_GEOMETRY_DESC* desc)
**功能**: 更新 Mesh 的顶点/索引数据并重建内部 BVH 树。

**参数**:
- `handle` - Mesh 几何句柄
- `desc` - 新的网格描述（顶点+索引）

**返回值**:
- `STATUS_SUCCESS` - 更新成功
- `STATUS_INVALID_HANDLE` - 句柄无效或不是 Mesh 类型
- `STATUS_INVALID_PARAMETER` - 描述结构非法

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 适用于动态网格场景，更新后会触发 BVH 重建。

---

### BOOLEAN FclIsGeometryHandleValid(FCL_GEOMETRY_HANDLE handle)
**功能**: 检查几何句柄是否存在于几何管理模块的注册表中。

**参数**:
- `handle` - 要检查的句柄

**返回值**:
- `TRUE` - 句柄有效
- `FALSE` - 句柄无效或已销毁

**IRQL要求**: 任意IRQL（只读操作）

---

### NTSTATUS FclAcquireGeometryReference(FCL_GEOMETRY_HANDLE handle, FCL_GEOMETRY_REFERENCE* reference, FCL_GEOMETRY_SNAPSHOT* snapshot)
**功能**: 获取几何引用和快照，用于碰撞/距离/CCD 计算。

**参数**:
- `handle` - 几何句柄
- `reference` - 输出参数，返回引用对象
- `snapshot` - 输出参数，返回几何快照（包含类型、指针等）

**返回值**:
- `STATUS_SUCCESS` - 获取成功
- `STATUS_INVALID_HANDLE` - 句柄无效

**IRQL要求**: 任意IRQL（使用共享读锁）

**说明**:
- 引用计数在此调用中递增
- 快照包含几何数据的只读视图
- 使用完毕后必须调用 `FclReleaseGeometryReference` 释放

---

### VOID FclReleaseGeometryReference(FCL_GEOMETRY_REFERENCE* reference)
**功能**: 释放几何引用，递减引用计数。

**参数**:
- `reference` - 之前通过 `FclAcquireGeometryReference` 获取的引用

**返回值**: 无

**IRQL要求**: 任意IRQL

**说明**: 当引用计数降为0时，几何对象可被真正删除。

---

## 碰撞检测 API

### NTSTATUS FclCollisionDetect(FCL_GEOMETRY_HANDLE object1, const FCL_TRANSFORM* transform1, FCL_GEOMETRY_HANDLE object2, const FCL_TRANSFORM* transform2, FCL_CONTACT_INFO* contact)
**功能**: 执行离散碰撞检测，调用 upstream FCL 算法。

**参数**:
- `object1` / `object2` - 两个几何对象的句柄
- `transform1` / `transform2` - 对象的变换（旋转+平移）
- `contact` - 输出参数，返回碰撞接触信息

**返回值**:
- `STATUS_SUCCESS` - 检测完成（碰撞结果在 `contact` 中）
- `STATUS_INVALID_HANDLE` - 句柄无效
- `STATUS_INVALID_PARAMETER` - 变换非法（非有限值、非正交矩阵等）

**IRQL要求**: `PASSIVE_LEVEL`

**支持的几何组合**: Sphere-Sphere, Sphere-OBB, Sphere-Mesh, Mesh-Mesh（通过 upstream FCL）

**说明**:
- 接触信息包括接触点、法向量、穿透深度
- 自动记录性能统计（可通过 `IOCTL_FCL_QUERY_DIAGNOSTICS` 查询）

---

### NTSTATUS FclCollideObjects(const FCL_COLLISION_OBJECT_DESC* object1, const FCL_COLLISION_OBJECT_DESC* object2, FCL_COLLISION_QUERY_RESULT* result)
**功能**: 高级碰撞检测接口，封装了几何+变换的描述。

**参数**:
- `object1` / `object2` - 碰撞对象描述（几何句柄+变换）
- `result` - 输出参数，返回碰撞查询结果（是否碰撞+接触信息）

**返回值**:
- `STATUS_SUCCESS` - 查询成功
- `STATUS_INVALID_PARAMETER` - 对象描述非法

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 适合直接在 IOCTL 层使用的便捷接口。

---

## 距离计算 API

### NTSTATUS FclDistanceCompute(FCL_GEOMETRY_HANDLE object1, const FCL_TRANSFORM* transform1, FCL_GEOMETRY_HANDLE object2, const FCL_TRANSFORM* transform2, FCL_DISTANCE_RESULT* result)
**功能**: 计算两个几何对象间的最短距离及最近点对。

**参数**:
- `object1` / `object2` - 几何句柄
- `transform1` / `transform2` - 对象变换
- `result` - 输出参数，包含：
  - `Distance` - 最短距离
  - `ClosestPoint1` / `ClosestPoint2` - 最近点坐标

**返回值**:
- `STATUS_SUCCESS` - 计算成功
- `STATUS_INVALID_HANDLE` - 句柄无效
- `STATUS_INVALID_PARAMETER` - 变换非法

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 调用 upstream FCL 的 `fcl::distance()` 算法。

---

## 连续碰撞检测（CCD）API

### NTSTATUS FclInterpMotionInitialize(const FCL_INTERP_MOTION_DESC* desc, FCL_INTERP_MOTION* motion)
**功能**: 初始化插值运动描述符（起止位姿的线性插值）。

**参数**:
- `desc` - 运动描述（StartTransform, EndTransform）
- `motion` - 输出参数，返回初始化的运动对象

**返回值**:
- `STATUS_SUCCESS` - 初始化成功
- `STATUS_INVALID_PARAMETER` - 变换非法

**IRQL要求**: `PASSIVE_LEVEL`

---

### NTSTATUS FclInterpMotionEvaluate(const FCL_INTERP_MOTION* motion, double t, FCL_TRANSFORM* transform)
**功能**: 在 `[0,1]` 时间范围内评估插值运动的中间位姿。

**参数**:
- `motion` - 运动对象
- `t` - 时间参数（0 = 起始，1 = 结束）
- `transform` - 输出参数，返回插值后的变换

**返回值**:
- `STATUS_SUCCESS` - 评估成功
- `STATUS_INVALID_PARAMETER` - t 超出范围

**IRQL要求**: `PASSIVE_LEVEL`

---

### NTSTATUS FclScrewMotionInitialize(const FCL_SCREW_MOTION_DESC* desc, FCL_SCREW_MOTION* motion)
**功能**: 初始化螺旋运动描述符（起止位姿 + 轴 + 速度参数）。

**参数**:
- `desc` - 螺旋运动描述
- `motion` - 输出参数

**返回值**:
- `STATUS_SUCCESS` - 初始化成功

**IRQL要求**: `PASSIVE_LEVEL`

---

### NTSTATUS FclScrewMotionEvaluate(const FCL_SCREW_MOTION* motion, double t, FCL_TRANSFORM* transform)
**功能**: 在 `[0,1]` 时间范围内评估螺旋运动。

**参数**:
- `motion` - 螺旋运动对象
- `t` - 时间参数
- `transform` - 输出参数

**返回值**:
- `STATUS_SUCCESS` - 评估成功

**IRQL要求**: `PASSIVE_LEVEL`

---

### NTSTATUS FclContinuousCollision(const FCL_CONTINUOUS_COLLISION_QUERY* query, FCL_CONTINUOUS_COLLISION_RESULT* result)
**功能**: 执行连续碰撞检测（Conservative Advancement 算法）。

**参数**:
- `query` - CCD 查询描述：
  - `Object1` / `Object2` - 几何句柄
  - `Motion1` / `Motion2` - 运动描述（InterpMotion 或 ScrewMotion）
  - `Tolerance` - 容差（0 表示使用默认值）
  - `MaxIterations` - 最大迭代次数（0 表示使用默认值）
- `result` - 输出参数，包含：
  - `IsColliding` - 是否发生碰撞
  - `TimeOfImpact` (TOI) - 碰撞时刻（0-1 范围）
  - `Contact` - 碰撞时的接触信息

**返回值**:
- `STATUS_SUCCESS` - CCD 计算成功
- `STATUS_INVALID_HANDLE` - 几何句柄无效
- `STATUS_INVALID_PARAMETER` - 运动描述非法

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 调用 upstream FCL 的 `fcl::continuousCollide()`，支持 InterpMotion 和 ScrewMotion。

---

## 周期性碰撞 API

### NTSTATUS FclStartPeriodicCollision(FCL_GEOMETRY_HANDLE object1, const FCL_TRANSFORM* transform1, FCL_GEOMETRY_HANDLE object2, const FCL_TRANSFORM* transform2, ULONG periodMicroseconds, FCL_PERIODIC_COLLISION_CALLBACK callback, PVOID context)
**功能**: 启动周期性碰撞检测（DPC + PASSIVE 两级模型）。

**参数**:
- `object1` / `object2` - 几何句柄
- `transform1` / `transform2` - 初始变换
- `periodMicroseconds` - 周期（微秒）
- `callback` - 回调函数（在 PASSIVE_LEVEL 执行）
- `context` - 用户上下文指针

**返回值**:
- `STATUS_SUCCESS` - 启动成功
- `STATUS_INVALID_HANDLE` - 句柄无效
- `STATUS_ALREADY_INITIALIZED` - 周期检测已在运行

**IRQL要求**: `PASSIVE_LEVEL`

**说明**:
- DPC 定时器在 DISPATCH_LEVEL 触发事件
- 内核工作线程在 PASSIVE_LEVEL 等待事件并执行回调
- 回调函数内可以安全地执行碰撞检测和其他 PASSIVE_LEVEL 操作

---

### VOID FclStopPeriodicCollision()
**功能**: 停止周期性碰撞检测。

**返回值**: 无

**IRQL要求**: `PASSIVE_LEVEL`

**说明**: 会等待工作线程安全退出，确保没有挂起的回调。

---

## 自测与健康检查

### NTSTATUS FclRunSelfTest(FCL_SELF_TEST_RESULT* result)
**功能**: 执行端到端自测，涵盖初始化 / 几何 / 碰撞 / CCD / 压力测试 / Verifier 兼容性等场景。

**参数**:
- `result` - 输出参数，包含每个子测试的详细状态（约18个字段）

**返回值**:
- `STATUS_SUCCESS` - 自测执行成功（但需检查 `result->Passed` 字段）
- `STATUS_UNSUCCESSFUL` - 自测执行失败

**IRQL要求**: `PASSIVE_LEVEL`

**测试项目**:
1. Initialize - Musa.Runtime 和内存池初始化
2. GeometryCreate - Sphere/OBB/Mesh 创建
3. Collision - 基础碰撞检测
4. Destroy - 几何销毁和句柄清理
5. Distance - 距离计算
6. Broadphase - AABB 树过滤
7. MeshGjk - Mesh-Sphere GJK 检测
8. SphereMesh - Sphere-Mesh 碰撞
9. MeshBroadphase - 网格宽相
10. ContinuousCollision - CCD (InterpMotion)
11. GeometryUpdate - Mesh 更新
12. SphereObb - Sphere-OBB 碰撞
13. MeshComplex - 复杂网格场景
14. Boundary - 边界条件
15. DriverVerifier - Driver Verifier 兼容性
16. LeakTest - 内存泄漏检测
17. StressTest - 压力测试（1M+ 调用）
18. Performance - 性能基准

**说明**:
- 自测前后会记录池统计（`PoolBefore` / `PoolAfter`）
- `PoolBalanced` 字段指示内存是否平衡
- 适合在驱动加载后立即调用以验证环境

---

### NTSTATUS FclRunSelfTestScenario(FCL_SELF_TEST_SCENARIO_ID scenarioId, FCL_SELF_TEST_SCENARIO_RESULT* result)
**功能**: 执行单一场景的自检。

**参数**:
- `scenarioId` - 场景ID：
  - `Runtime` (1) - Musa.Runtime 功能验证
  - `SphereCollision` (2) - 球体碰撞
  - `Broadphase` (3) - AABB 树过滤
  - `MeshCollision` (4) - 网格碰撞
  - `Ccd` (5) - 连续碰撞检测
- `result` - 输出参数，包含场景测试的详细步骤信息

**返回值**:
- `STATUS_SUCCESS` - 场景测试完成
- `STATUS_INVALID_PARAMETER` - 场景ID 非法

**IRQL要求**: `PASSIVE_LEVEL`

---

### NTSTATUS FclQueryHealth(FCL_PING_RESPONSE* response)
**功能**: 查询驱动健康状态（版本、初始化状态、内存池统计）。

**参数**:
- `response` - 输出参数，包含：
  - `Version` - 驱动版本（Major.Minor.Patch.Build）
  - `IsInitialized` - 是否已初始化
  - `CurrentTime` - 当前系统时间
  - `PoolStats` - 内存池统计（分配/释放次数、峰值使用等）

**返回值**:
- `STATUS_SUCCESS` - 查询成功

**IRQL要求**: 任意IRQL

**说明**: 对应 `IOCTL_FCL_PING`，用于健康检查和监控。

---

### NTSTATUS FclQueryDiagnostics(FCL_DIAGNOSTICS_RESPONSE* response)
**功能**: 查询性能诊断信息（碰撞/距离/CCD 的计时统计）。

**参数**:
- `response` - 输出参数，包含：
  - `CollisionTiming` - 碰撞检测性能统计
  - `DistanceTiming` - 距离计算性能统计
  - `CcdTiming` - CCD 性能统计
  - `DpcCollisionTiming` - DPC 级别碰撞统计
  - 每个统计包含：CallCount（调用次数）、TotalDuration（总耗时）、MinDuration（最小耗时）、MaxDuration（最大耗时）

**返回值**:
- `STATUS_SUCCESS` - 查询成功

**IRQL要求**: 任意IRQL

**说明**:
- 对应 `IOCTL_FCL_QUERY_DIAGNOSTICS`
- 用于性能分析和调优
- DPC 级别统计仅在周期碰撞模式下有意义

---

## 数据结构定义

### FCL_TRANSFORM
```c
typedef struct _FCL_TRANSFORM {
    FCL_MATRIX3X3 Rotation;      // 3x3 旋转矩阵
    FCL_VECTOR3 Translation;      // 3D 平移向量
} FCL_TRANSFORM;
```

### FCL_CONTACT_INFO
```c
typedef struct _FCL_CONTACT_INFO {
    BOOLEAN IsColliding;              // 是否发生碰撞
    ULONG ContactPointCount;          // 接触点数量
    FCL_VECTOR3 ContactPoints[8];     // 接触点坐标（最多8个）
    FCL_VECTOR3 ContactNormals[8];    // 接触法向量
    DOUBLE PenetrationDepths[8];      // 穿透深度
} FCL_CONTACT_INFO;
```

### FCL_DISTANCE_RESULT
```c
typedef struct _FCL_DISTANCE_RESULT {
    DOUBLE Distance;                  // 最短距离
    FCL_VECTOR3 ClosestPoint1;        // 对象1上的最近点
    FCL_VECTOR3 ClosestPoint2;        // 对象2上的最近点
} FCL_DISTANCE_RESULT;
```

### FCL_CONTINUOUS_COLLISION_RESULT
```c
typedef struct _FCL_CONTINUOUS_COLLISION_RESULT {
    BOOLEAN IsColliding;              // 是否在运动过程中碰撞
    DOUBLE TimeOfImpact;              // 碰撞时刻（0-1范围）
    FCL_CONTACT_INFO Contact;         // 碰撞时的接触信息
} FCL_CONTINUOUS_COLLISION_RESULT;
```

### FCL_POOL_STATS
```c
typedef struct _FCL_POOL_STATS {
    ULONGLONG AllocationCount;        // 总分配次数
    ULONGLONG FreeCount;              // 总释放次数
    ULONGLONG BytesAllocated;         // 总分配字节数
    ULONGLONG BytesFreed;             // 总释放字节数
    ULONGLONG BytesInUse;             // 当前使用字节数
    ULONGLONG PeakBytesInUse;         // 峰值使用字节数
} FCL_POOL_STATS;
```

---

## 完整的 API 清单

### 初始化
- `FclInitialize()` - 初始化驱动
- `FclCleanup()` - 清理驱动

### 几何管理
- `FclCreateGeometry()` - 创建几何对象
- `FclDestroyGeometry()` - 销毁几何对象
- `FclUpdateMeshGeometry()` - 更新网格数据
- `FclIsGeometryHandleValid()` - 验证句柄
- `FclAcquireGeometryReference()` - 获取引用和快照
- `FclReleaseGeometryReference()` - 释放引用

### 碰撞检测
- `FclCollisionDetect()` - 基础碰撞检测
- `FclCollideObjects()` - 高级碰撞接口

### 距离计算
- `FclDistanceCompute()` - 距离查询

### 连续碰撞
- `FclInterpMotionInitialize()` - 初始化插值运动
- `FclInterpMotionEvaluate()` - 评估插值运动
- `FclScrewMotionInitialize()` - 初始化螺旋运动
- `FclScrewMotionEvaluate()` - 评估螺旋运动
- `FclContinuousCollision()` - 执行 CCD

### 周期碰撞
- `FclStartPeriodicCollision()` - 启动周期检测
- `FclStopPeriodicCollision()` - 停止周期检测

### 自检与诊断
- `FclRunSelfTest()` - 完整自检
- `FclRunSelfTestScenario()` - 场景自检
- `FclQueryHealth()` - 健康检查
- `FclQueryDiagnostics()` - 性能诊断

---

## 使用示例

### 基础碰撞检测

```c
// 1. 初始化
NTSTATUS status = FclInitialize();
if (!NT_SUCCESS(status)) return status;

// 2. 创建两个球体
FCL_SPHERE_GEOMETRY_DESC sphere1 = { .Center = {0, 0, 0}, .Radius = 1.0 };
FCL_SPHERE_GEOMETRY_DESC sphere2 = { .Center = {1.5, 0, 0}, .Radius = 1.0 };

FCL_GEOMETRY_HANDLE handle1, handle2;
FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphere1, &handle1);
FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphere2, &handle2);

// 3. 设置变换（单位变换）
FCL_TRANSFORM transform = {
    .Rotation = {{1,0,0}, {0,1,0}, {0,0,1}},
    .Translation = {0,0,0}
};

// 4. 执行碰撞检测
FCL_CONTACT_INFO contact;
status = FclCollisionDetect(handle1, &transform, handle2, &transform, &contact);

if (NT_SUCCESS(status) && contact.IsColliding) {
    KdPrint(("Collision detected! Penetration depth: %f\n",
             contact.PenetrationDepths[0]));
}

// 5. 清理
FclDestroyGeometry(handle1);
FclDestroyGeometry(handle2);
FclCleanup();
```

### 连续碰撞检测（CCD）

```c
// 初始化运动描述
FCL_TRANSFORM start = { /* ... */ };
FCL_TRANSFORM end = { /* ... */ };

FCL_INTERP_MOTION_DESC motionDesc = {
    .StartTransform = start,
    .EndTransform = end
};

FCL_INTERP_MOTION motion;
FclInterpMotionInitialize(&motionDesc, &motion);

// CCD 查询
FCL_CONTINUOUS_COLLISION_QUERY query = {
    .Object1 = handle1,
    .Object2 = handle2,
    .Motion1 = &motion,
    .Motion2 = NULL, // 静止对象
    .Tolerance = 1e-6,
    .MaxIterations = 100
};

FCL_CONTINUOUS_COLLISION_RESULT result;
FclContinuousCollision(&query, &result);

if (result.IsColliding) {
    KdPrint(("Collision at TOI: %f\n", result.TimeOfImpact));
}
```

### 周期性碰撞

```c
// 回调函数（在 PASSIVE_LEVEL 执行）
VOID NTAPI MyCollisionCallback(
    BOOLEAN isColliding,
    const FCL_CONTACT_INFO* contact,
    PVOID context
) {
    if (isColliding) {
        KdPrint(("Periodic collision detected!\n"));
    }
}

// 启动周期检测（1ms 周期）
FclStartPeriodicCollision(
    handle1, &transform1,
    handle2, &transform2,
    1000,  // 1ms = 1000us
    MyCollisionCallback,
    NULL   // context
);

// ... 执行其他任务 ...

// 停止周期检测
FclStopPeriodicCollision();
```

---

## 注意事项

1. **IRQL 要求**：
   - 大多数 API 必须在 `PASSIVE_LEVEL` 调用
   - 快照 API（使用 `FCL_GEOMETRY_SNAPSHOT`）可在 `DISPATCH_LEVEL` 调用
   - 周期碰撞回调在 `PASSIVE_LEVEL` 执行

2. **内存管理**：
   - 所有几何对象使用 NonPagedPool 分配
   - 引用计数机制确保对象在使用时不会被删除
   - 必须配对调用 `AcquireReference` 和 `ReleaseReference`

3. **线程安全**：
   - 几何管理器使用 EX_PUSH_LOCK 保护
   - 碰撞检测使用只读快照，无需额外锁定
   - 周期碰撞使用事件同步机制

4. **性能考虑**：
   - Mesh 几何会自动构建 BVH，创建时有开销
   - 使用快照可以避免每次检测时的锁定开销
   - 周期碰撞在 DPC 定时器触发，适合实时场景

5. **错误处理**：
   - 所有 API 返回 NTSTATUS
   - 使用 `NT_SUCCESS()` 宏检查返回值
   - 异常会被捕获并转换为错误码

---

## 参考资料

- **IOCTL 定义**: `kernel/core/include/fclmusa/ioctl.h`
- **几何类型**: `kernel/core/include/fclmusa/geometry.h`
- **碰撞 API**: `kernel/core/include/fclmusa/collision.h`
- **距离 API**: `kernel/core/include/fclmusa/distance.h`
- **自测 API**: `kernel/core/include/fclmusa/self_test.h`
- **架构说明**: `docs/architecture.md`
- **使用指南**: `docs/usage.md`
