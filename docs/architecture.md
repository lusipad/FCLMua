FCL+Musa 架构说明
=================

## 分层总览

FCL+Musa 在内核侧按三层结构组织，与用户态只通过 IOCTL 交互：

1. 第 0 层：upstream FCL（`fcl-source` + Eigen + libccd）
   - 提供所有几何 / 碰撞 / 距离 / CCD 的核心算法实现；
   - 使用原生 `fcl::collide`、`fcl::distance`、`fcl::continuousCollide` 等接口；
   - 不感知 IRP/设备对象，仅处理数学和几何问题。

2. 第 1 层：FCL 内核管理模块（FclCore）
   - 位置：`kernel/core/src` 下的 `geometry/`、`collision/`、`distance/`、`broadphase/`、`testing/` 等目录；
   - 职责：
     - 几何对象生命周期管理：`FclCreateGeometry` / `FclDestroyGeometry` / `FclUpdateMeshGeometry` / `FclAcquireGeometryReference` / `FclReleaseGeometryReference`；
     - BVH 构建与缓存（`geometry/bvh_model.cpp` 等，仅作为 upstream FCL 的数据准备层）；
     - 碰撞 / 距离 / CCD 调用流程：`FclCollisionDetect` / `FclCollideObjects` / `FclDistanceCompute` / `FclContinuousCollision`；
     - 宽阶段碰撞对收集：`FclBroadphaseDetect`；
     - 自测与健康检查：`FclRunSelfTest` / `FclQueryHealth`；
     - 内存池与统计：`memory/pool_allocator.cpp` / `FCL_POOL_STATS`。
   - 要点：
     - 不重新实现 GJK/EPA 等碰撞算法，而是通过 upstream bridge 调用 upstream FCL 算法；
     - 向上暴露纯 C + NTSTATUS 风格的 API，不直接操作 IRP、设备对象。

3. 第 2 层：驱动 / IOCTL 封装层
   - 位置：`kernel/driver/src/driver_entry.cpp`、`kernel/driver/src/device_control.cpp`；
   - 职责：
     - 处理 `IRP_MJ_CREATE/CLOSE/DEVICE_CONTROL`；
     - 校验 IOCTL 输入/输出缓冲区长度与对齐；
     - 将 `SystemBuffer` 解释为 `fclmusa/ioctl.h` 中定义的结构体，并调用第 1 层的 FCL 管理 API；
     - 不持有几何对象或控制算法状态，所有几何/碰撞/CCD 状态由第 1 层管理。

## 模块概览（按职能划分）

- 内存系统：`kernel/core/src/memory/pool_allocator.cpp`
  - 提供 NonPagedPool 上的 RAII 分配器和全局统计，用于 STL/Eigen/libccd 等依赖。

- 几何管理：`kernel/core/src/geometry/geometry_manager.cpp` 等
  - 负责 Sphere / OBB / Mesh 对象的创建、查找、引用计数和销毁；
  - Mesh 几何会在必要时构建 BVH（`kernel/core/src/geometry/bvh_model.cpp`），作为 upstream FCL 使用的包围体结构。

- 碰撞 / 距离 / CCD：
  - `kernel/core/src/collision/collision.cpp`
  - `kernel/core/src/collision/continuous_collision.cpp`
  - `kernel/core/src/distance/distance.cpp`
  - 统一从几何管理层获取 `FCL_GEOMETRY_SNAPSHOT`；
  - 对输入变换做基本合法性校验（有限值、正交矩阵等）；
  - 通过 upstream bridge 调用 upstream FCL 的碰撞 / 距离 / 连续碰撞算法；
  - 将结果封装为 `FCL_CONTACT_INFO` / `FCL_DISTANCE_RESULT` / `FCL_CONTINUOUS_COLLISION_RESULT` 结构。

- 宽阶段：`kernel/core/src/broadphase/broadphase.cpp`
  - 基于 upstream FCL 的 `DynamicAABBTreeCollisionManagerd` 实现宽阶段对收集；
  - 利用几何管理层提供的快照和绑定信息构造 `fcl::CollisionObjectd`，输出 `FCL_BROADPHASE_PAIR`。

- 自测：`kernel/core/src/testing/self_test.cpp` 等
  - 组合调用几何 / 碰撞 / 距离 / CCD / 宽阶段等 API；
  - 验证核心行为和边界条件，并聚合为 `FCL_SELF_TEST_RESULT` 结构；
  - 为 IOCTL `IOCTL_FCL_SELF_TEST` 提供实现基础。

## 调用链示例

1. 用户态通过 IOCTL 查询碰撞
   - Ring3：构造 `FCL_COLLISION_IO_BUFFER`，调用 `IOCTL_FCL_QUERY_COLLISION`；
   - 驱动（第 2 层）：`device_control.cpp:FclDispatchDeviceControl` → `HandleCollisionQuery`；
   - FCL 管理模块（第 1 层）：调用 `FclCollisionDetect`，内部通过几何管理获取快照；
   - upstream FCL（第 0 层）：通过 `kernel/core/src/upstream/upstream_bridge.cpp` 调用 `fcl::collide` 完成实际碰撞计算；
   - 结果沿相同路径返回，最终写入 `FCL_COLLISION_IO_BUFFER::Result`。

2. Ring0 直接使用 FCL API
   - 内核代码在 `PASSIVE_LEVEL` 下直接调用 `FclCreateGeometry` / `FclCollisionDetect` 等 API；
   - 调用路径与 IOCTL 相同，只是跳过了 IOCTL/IRP 层，直接与第 1 层 FCL 管理模块交互。

## 关键依赖

- Musa.Runtime：为内核环境提供 STL/异常/线程本地存储等运行时基础。
- Eigen：通过 `math/eigen_config.h` 做内核兼容包装，支撑 OBBRSS 等几何/线性代数运算。
- libccd：内嵌于 `external/libccd`，提供 GJK/EPA 支持，由 upstream FCL 间接使用。

## 安全 / 稳定性考量

- 所有内存分配均基于 NonPagedPool，由 `FCL_POOL_STATS` 做实时统计；
- 严格区分 IRQL 级别，碰撞 / 距离 / CCD 等 API 要求在 `PASSIVE_LEVEL` 调用；
- Driver Verifier 通过自测结构暴露泄漏/异常，避免内核崩溃并提升可观测性。

## Upstream FCL 集成说明

- 通过 `kernel/core/src/upstream/upstream_bridge.cpp` / `kernel/core/src/upstream/geometry_bridge.cpp` 将 upstream FCL（`fcl-source`）的算法适配到内核环境：
  - 屏蔽异常，转换为 NTSTATUS；
  - 统一日志和内存分配路径；
  - 处理内核可接受的浮点精度和数据布局。
- `FclCollisionDetect` / `FclDistanceCompute` / `FclContinuousCollision` / `FclBroadphaseDetect` 等 API 统一使用 upstream FCL 的 `collide`、`distance`、`continuousCollide`、`DynamicAABBTreeCollisionManagerd` 等实现。
- Mesh 几何构建时，会根据需要生成与 upstream FCL 兼容的 BVH 结构，以便其宽阶段 / 窄阶段算法使用。

