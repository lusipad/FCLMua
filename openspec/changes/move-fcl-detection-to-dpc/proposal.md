## Why

当前实现刻意将所有 FCL 重计算（碰撞 / 距离 / CCD / 广义 broadphase）限制在 PASSIVE_LEVEL：

- `FclCollisionDetect` / `FclDistanceCompute` / `FclContinuousCollision` 入口都强制 `KeGetCurrentIrql() == PASSIVE_LEVEL`；
- 几何管理（`FclCreateGeometry` / `FclUpdateMeshGeometry` / `FclAcquireGeometryReference`）通过 `EX_PUSH_LOCK + AVL` 管理 NonPagedPool 中的 mesh / BVH，同样是 PASSIVE-only；
- 周期碰撞调度器（`periodic_scheduler.*`）采用 DPC 仅唤醒内核线程的模式，把所有 FCL 逻辑留在 PASSIVE 线程执行。

这种设计确保了 API 使用简单、锁模型统一，但也有明显限制：

- 无法在 DPC 上运行 FCL 碰撞/CCD 计算，难以满足“控制环周期和最坏延迟有硬约束”的实时场景；
- 想要在 ISR 中消费上一周期的碰撞/CCD 结果，缺少一个“DPC-safe 的核心计算 + 结果快照”层；
- 周期碰撞当前是 DPC+PASSIVE 混合路径，后续若要统一将 FCL 计算迁移到 DPC，仅靠现有 API 难以逐步演进。

我们希望引入一层明确的“Snapshot Core 层”，将 FCL 的重计算内核与几何/句柄/锁管理解耦，让：

- 所有 FCL 计算（碰撞 / 距离 / CCD / broadphase）在控制环内可以在 DPC 上执行；
- 所有资源与配置管理（几何创建/更新、handle 生命周期）保持在 PASSIVE 线程；
- 为未来在 ISR 中消费“上一周期结果”预留一个清晰的路径，但不在本 change 中一次性做完。


## What Changes

本 change 的目标是：**将 FCL 检测类计算迁移到 DPC 友好的核心层**，同时保持现有 API 兼容，对上层 R0 运动规划和 IOCTL 调用保持接口不变。

核心改动分三部分：

1. **引入 Snapshot Core 层（IRQL <= DISPATCH）**

   在 `kernel/core` 下为 FCL 检测模块抽象出一组“仅使用 snapshot/motion 的核心 API”，不再直接依赖 handle 或 PushLock：

   - 离散碰撞：
     - `FclCollisionCoreFromSnapshots`：接受 `FCL_GEOMETRY_SNAPSHOT` + `FCL_TRANSFORM`，内部直接调用 `FclUpstreamCollide` 并记录诊断；
   - 距离：
     - `FclDistanceCoreFromSnapshots`：接受 snapshot + transform，调用 `FclUpstreamDistance`；
   - CCD：
     - `FclContinuousCollisionCoreFromSnapshots`：接受 snapshot + motion（`FCL_INTERP_MOTION`）、tolerance/iterations，调用 `FclUpstreamContinuousCollision`；
   - broadphase：
     - 抽象 `FclBroadphaseCoreFromBindings`，接受事先绑定好的 `GeometryBinding` / `CollisionObject` 列表，避免在 DPC 内执行 `FclAcquireGeometryReference` 或重建 dynamic AABB tree；
     - 由 PASSIVE 层负责构建和维护 broadphase context（例如对象集合和 FCL dynamic AABB tree 的生命周期），Core 层只负责在 DPC 中执行 broadphase 检测。

   这些 Core API 的约束：

   - 不做 IRQL 检查，由调用方保证在 `<= DISPATCH_LEVEL`；
   - 只访问 NonPagedPool 中的数据（snapshot / BVH / GeometryBinding），不访问分页内存；
   - 不使用 PushLock / AVL / 任何 PASSIVE-only 锁，仅依赖 C++ 运行时和 `Interlocked`。

2. **保留并重构现有 PASSIVE API 为薄包装**

   在确保上层行为不变的前提下，重构现有 FCL API，使其成为对 Core 层的 PASSIVE 包装器：

   - `FclCollisionDetect` / `FclDistanceCompute` / `FclContinuousCollision`：
     - 保留现有签名和 PASSIVE-only 语义，继续面的 IOCTL / 自测 / 规划使用；
     - 内部改为：
       - 检查 IRQL == PASSIVE_LEVEL；
       - 通过 `FclAcquireGeometryReference` 从 handle 获取 `FCL_GEOMETRY_SNAPSHOT`；
       - 调用对应的 `*CoreFromSnapshots` 完成实际 FCL 计算；
       - 函数退出时通过析构释放引用。
   - broadphase：
     - 如抽象了 Broadphase Core，则 `FclBroadphaseDetect` 成为：
       - PASSIVE-only API，负责从 handle 获取 snapshot + 构建/更新 `ManagedObject` / `GeometryBinding`；
       - 调用 Broadphase Core 完成 DPC-safe 的 FCL broadphase 计算。

   通过这种改造：

   - 所有现有内核调用和 IOCTL 行为保持兼容；
   - Core 层为 DPC/ISR 消费者提供了清晰的“无 handle / 无锁”的入口；
   - 几何管理子系统的 IRQL 和锁约束保持不变，风险集中在新增的 Core 层实现上。

3. **为 DPC 使用定义共享上下文与结果快照模式**

   在 driver 层定义一套 DPC 友好的上下文结构和读写协议，使 DPC 能够安全执行 Core 层计算并将结果暴露给：

   - 控制环内的 DPC 本身（例如多个周期内叠加 CCD 判定）；
   - PASSIVE 线程上的运动规划逻辑；
   - 后续可能的 ISR 快路径（仅消费上一周期结果，不在本 change 内实现）。

   设计要点：

   - 上下文结构示例（简化）：
     - 包含：`FCL_GEOMETRY_SNAPSHOT*` 指针或内嵌 snapshot、当前周期的 transform/motion、最后一次离散碰撞/CCD 结果、`volatile ULONGLONG Sequence`；
     - 存放在 NonPagedPool，DPC 和内核线程共享；
   - DPC 写结果时：
     - 先写结果结构体，再调用 `InterlockedIncrement64(&Sequence)`；
     - 不使用 PushLock / KeWait 等 PASSIVE-only 原语；
   - 内核线程 / ISR 读结果时：
     - 使用“两次读 sequence”模式避免半写：
       - 读 `s1 = Sequence` → 拷贝结果结构体 → 再读 `s2 = Sequence`；
       - `s1 == s2` 且非零时，拷贝即为一致快照；
   - 该模式统一适用于离散碰撞、距离和 CCD，便于在后续 change 中拓展 ISR 读取。


## Impact

此 change 主要影响以下方面：

- **Kernel FCL 模块结构**：
  - 从“PASSIVE-only API + 内部直接调用 upstream FCL”演变为“三层结构”：
    - 几何/句柄管理（PASSIVE-only）
    - Snapshot Core（IRQL <= DISPATCH）
    - 调度/上下文层（DPC + 内核线程）
  - 对调用者而言，现有 `Fcl*` API 保持兼容，但新增了一组内部 Core API 用于 DPC。

- **周期调度与实时路径**：
  - 现有 `periodic_scheduler` 和周期碰撞逻辑可以迁移为“DPC 直接执行 Core 计算”，不再依赖 PASSIVE 工作线程执行 FCL 重计算；
  - 控制环可以将所有 FCL 检测（碰撞 / 距离 / CCD）放在 DPC 路径执行，从而获得更稳定的延迟上界；
  - 内核线程从“计算+决策”演变为“读取 DPC 结果 + 进行高层规划与几何更新”。

- **IRQL 与锁模型**：
  - Core 层禁用 PushLock、KeWait 等 PASSIVE-only 原语，只允许使用：
    - NonPagedPool 内存；
    - `Interlocked*` 原子操作；
    - upstream FCL / Eigen / libccd 已经适配的内核运行时。
  - 几何/句柄管理保留现有锁模型，仍然 PASSIVE-only，从而降低对成熟代码的影响。

- **未来扩展（非本 change 完成）**：
  - ISR 可在 DIRQL 中读取 DPC 产生的上一周期结果（通过 sequence+快照模式），用于极简紧急决策；
  - 可在后续 change 上增加更精细的“周期预算 / 超时诊断”，例如记录 DPC 内 FCL 调用的最坏/平均耗时并通过 IOCTL 暴露给用户态。

整体上，本 change 更像是一个“内核内 FCL 检测栈的分层重构”，为 DPC/ISR 场景提供稳定的抽象边界，同时保持现有用户态接口和大部分内核 API 的语义不变。
