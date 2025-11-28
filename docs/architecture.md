FCL+Musa 架构说明
=================

## 平台抽象

FCL+Musa 通过 `platform.h` 实现跨平台抽象，支持内核模式（R0）和用户模式（R3）的双重构建：

- **内核模式（R0）**：完整的驱动功能，依赖 WDK 环境，使用 NonPagedPool、EX_PUSH_LOCK、IRQL 等内核特性
- **用户模式（R3）**：静态库形式，可通过 CPM 集成到用户态项目，使用 SRWLOCK、标准 C++ 等用户态特性
- **条件编译**：通过 `FCL_MUSA_KERNEL_MODE` 宏控制，默认为 1（内核模式），用户态构建时设为 0

所有头文件统一使用 `#include "fclmusa/platform.h"` 而非直接包含 `<ntddk.h>`，确保跨平台兼容性。

## 分层总览

FCL+Musa 在内核侧按三层结构组织，与用户态只通过 IOCTL 交互：

1. **第 0 层：upstream FCL**（`fcl-source` + Eigen + libccd）
   - 提供所有几何 / 碰撞 / 距离 / CCD 的核心算法实现
   - 使用原生 `fcl::collide`、`fcl::distance`、`fcl::continuousCollide` 等接口
   - 不感知 IRP/设备对象，仅处理数学和几何问题
   - 平台无关，同时支持 R0 和 R3 环境

2. **第 1 层：FCL 内核管理模块**（FclCore）
   - **位置**：`kernel/core/src` 下的 `geometry/`、`collision/`、`distance/`、`broadphase/`、`testing/` 等目录
   - **职责**：
     - 几何对象生命周期管理：`FclCreateGeometry` / `FclDestroyGeometry` / `FclUpdateMeshGeometry` / `FclAcquireGeometryReference` / `FclReleaseGeometryReference`
     - BVH 构建与缓存（`geometry/bvh_model.cpp` 等，仅作为 upstream FCL 的数据准备层）
     - 碰撞 / 距离 / CCD 调用流程：`FclCollisionDetect` / `FclCollideObjects` / `FclDistanceCompute` / `FclContinuousCollision`
     - **周期碰撞调度**：`IOCTL_FCL_START_PERIODIC_COLLISION` / `IOCTL_FCL_STOP_PERIODIC_COLLISION`（由 DPC 定时执行）
     - 宽阶段碰撞对收集：`FclBroadphaseDetect`
     - 自测与健康检查：`FclRunSelfTest` / `FclRunSelfTestScenario` / `FclQueryHealth` / `FclQueryDiagnostics`
     - 内存池与统计：`memory/pool_allocator.cpp` / `FCL_POOL_STATS`
   - **要点**：
     - 不重新实现 GJK/EPA 等碰撞算法，而是通过 upstream bridge 调用 upstream FCL 算法
     - 向上暴露纯 C + NTSTATUS 风格的 API，不直接操作 IRP、设备对象
     - 支持在不同 IRQL 级别调用（大多数 API 要求 PASSIVE_LEVEL，快照 API 可在 DISPATCH_LEVEL）

3. **第 2 层：驱动 / IOCTL 封装层**
   - **位置**：`kernel/driver/src/driver_entry.cpp`、`kernel/driver/src/device_control.cpp`
   - **职责**：
     - 处理 `IRP_MJ_CREATE/CLOSE/DEVICE_CONTROL`
     - 校验 IOCTL 输入/输出缓冲区长度与对齐
     - 将 `SystemBuffer` 解释为 `fclmusa/ioctl.h` 中定义的结构体，并调用第 1 层的 FCL 管理 API
     - 不持有几何对象或控制算法状态，所有几何/碰撞/CCD 状态由第 1 层管理

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

- 周期碰撞调度逻辑：`kernel/driver/src/device_control.cpp` 中的 DPC 计时器实现
  - **FCL_PERIODIC_COLLISION_STATE**：在启动 IOCTL（PASSIVE_LEVEL）中获取几何引用、构造 `FCL_GEOMETRY_SNAPSHOT`、配置运动参数，并预分配 NonPaged Scratch 缓冲，随后由 DPC 周期性执行碰撞计算。
  - **DPC 回调**（`FclPeriodicCollisionDpc`）：在 DISPATCH_LEVEL 直接调用 `FclCollisionCoreFromSnapshots`，将结果写入 `LastResult`/`LastStatus`，并使用 `Sequence` 自增 + `KeMemoryBarrier` 形成“双读”快照协议（`FclPeriodicCollisionSnapshotResult`）；可根据 InnerIterations 在一次 DPC 内执行多轮检测。
  - **停止机制**：`IOCTL_FCL_STOP_PERIODIC_COLLISION` 取消计时器、等待 `DpcIdleEvent`，随后在 PASSIVE_LEVEL 释放引用/快照；实时碰撞逻辑始终留在 DPC，满足亚毫秒预算。
  - 适用于实时控制环境，需要周期性碰撞检测的场景；如果只需 PASSIVE 线程查询，可直接复用 Snapshot Core API。

- 自测：`kernel/core/src/testing/self_test.cpp` 等
  - 组合调用几何 / 碰撞 / 距离 / CCD / 宽阶段等 API
  - 支持完整自检（`FclRunSelfTest`）和场景自检（`FclRunSelfTestScenario`）
  - 验证核心行为和边界条件，并聚合为 `FCL_SELF_TEST_RESULT` 结构
  - 为 IOCTL `IOCTL_FCL_SELF_TEST` 和 `IOCTL_FCL_SELF_TEST_SCENARIO` 提供实现基础

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

- **platform.h**：平台抽象层，提供 R0/R3 统一的 API 接口（EX_PUSH_LOCK/SRWLOCK、NTSTATUS、日志宏等）
- **Musa.Runtime**：为内核环境提供 STL/异常/线程本地存储等运行时基础（仅 R0 需要）
- **Eigen**：通过 `math/eigen_config.h` 做内核兼容包装，支撑 OBBRSS 等几何/线性代数运算
- **libccd**：内嵌于 `external/libccd`，提供 GJK/EPA 支持，由 upstream FCL 间接使用

## 并发与 IRQL 管理

### IRQL 级别控制

1. **EX_PUSH_LOCK 读写锁**：用于保护几何表 (geometry_manager)
   - `ExAcquirePushLockShared` / `ExReleasePushLockShared`：读操作（获取引用）
   - `ExAcquirePushLockExclusive` / `ExReleasePushLockExclusive`：写操作（创建/销毁）

2. **周期调度 + Snapshot Core**：
   - PASSIVE 层只在启动/停止阶段执行，负责构建 `FCL_PERIODIC_COLLISION_STATE`、持有 `FCL_GEOMETRY_SNAPSHOT`，并准备 NonPaged Scratch；周期计算完全由 DPC 驱动。
   - `FclPeriodicCollisionDpc` 在 DISPATCH_LEVEL 直接调用 `FclCollisionCoreFromSnapshots`（必要时多次迭代），不访问任何 pageable 资源或需要 PushLock 的句柄表。
   - `Sequence` 原子计数结合 `FclPeriodicCollisionSnapshotResult` 的双读协议提供一致性，无需额外锁；上层按照序列值判断是否读到完整结果。

3. **原子性计时统计**：`volatile LONG64` 用于无锁更新检测性能数据
   ```c
   struct DetectionTimingAccumulator {
       volatile LONG64 CallCount;
       volatile LONG64 TotalDurationMicroseconds;
       volatile LONG64 MinDurationMicroseconds;
       volatile LONG64 MaxDurationMicroseconds;
   };
   ```

### 安全 / 稳定性考量

- 所有内存分配均基于 NonPagedPool，由 `FCL_POOL_STATS` 做实时统计
- 严格区分 IRQL 级别，碰撞 / 距离 / CCD 等 API 要求在 `PASSIVE_LEVEL` 调用
- 快照 API（使用 `FCL_GEOMETRY_SNAPSHOT`）可在 `DISPATCH_LEVEL` 调用
- 周期碰撞 DPC 仅依赖 Snapshot Core API，可在 DISPATCH_LEVEL 下安全运行；若业务需要 PASSIVE 语义，可在上层线程中调用 `FclPeriodicCollisionSnapshotResult` 读取 DPC 产出的结果快照
- Driver Verifier 通过自测结构暴露泄漏/异常，避免内核崩溃并提升可观测性

## Upstream FCL 集成说明

- 通过 `kernel/core/src/upstream/upstream_bridge.cpp` / `kernel/core/src/upstream/geometry_bridge.cpp` 将 upstream FCL（`fcl-source`）的算法适配到内核环境：
  - 屏蔽异常，转换为 NTSTATUS；
  - 统一日志和内存分配路径；
  - 处理内核可接受的浮点精度和数据布局。
- `FclCollisionDetect` / `FclDistanceCompute` / `FclContinuousCollision` / `FclBroadphaseDetect` 等 API 统一使用 upstream FCL 的 `collide`、`distance`、`continuousCollide`、`DynamicAABBTreeCollisionManagerd` 等实现。
- Mesh 几何构建时，会根据需要生成与 upstream FCL 兼容的 BVH 结构，以便其宽阶段 / 窄阶段算法使用。
