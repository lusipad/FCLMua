## 1. Snapshot Core 层抽象

- [ ] 1.1 在 `kernel/core/include` 中为 collision/distance/continuous_collision 定义 `*CoreFromSnapshots` 内部接口（仅使用 snapshot/motion，不暴露给外部 IOCTL）
- [ ] 1.2 在 `kernel/core/src/collision/collision.cpp` 中实现 `FclCollisionCoreFromSnapshots`，复用现有 `FclUpstreamCollide` + 计时/诊断逻辑
- [ ] 1.3 在 `kernel/core/src/distance/distance.cpp` 中实现 `FclDistanceCoreFromSnapshots`，复用 `FclUpstreamDistance` + 计时/诊断逻辑
- [ ] 1.4 在 `kernel/core/src/collision/continuous_collision.cpp` 中实现 `FclContinuousCollisionCoreFromSnapshots`，复用 `FclUpstreamContinuousCollision` + 计时/诊断逻辑
- [ ] 1.5 在 `kernel/core/src/broadphase/broadphase.cpp` 中抽象 `FclBroadphaseCoreFromBindings`（或等价接口），接受预构建的 `GeometryBinding` / `CollisionObject` 集合并执行 broadphase 检测
- [ ] 1.6 确保所有 Core 实现中不调用 `FclAcquireGeometryReference` / `FclReleaseGeometryReference` / PushLock / KeWait 系列，IRQL 注解允许 `<= DISPATCH_LEVEL`

## 2. PASSIVE API 重构为 Core 包装器

- [ ] 2.1 重构 `FclCollisionDetect`：
  - 2.1.1 保留现有签名与 PASSIVE-only 语义（IRQL 检查不变）
  - 2.1.2 使用 `FclAcquireGeometryReference` 获取 `FCL_GEOMETRY_SNAPSHOT`，调用 `FclCollisionCoreFromSnapshots` 完成计算
  - 2.1.3 确保失败路径与当前实现行为一致（包括诊断统计）
- [ ] 2.2 重构 `FclDistanceCompute` 为 `FclDistanceCoreFromSnapshots` 的 PASSIVE 包装器，保证错误码与现有实现兼容
- [ ] 2.3 重构 `FclContinuousCollision` 为 `FclContinuousCollisionCoreFromSnapshots` 的 PASSIVE 包装器，保留对 tolerance/iterations 的处理与诊断记录
- [ ] 2.4 重构 `FclBroadphaseDetect`：
  - 2.4.1 保留现有 PASSIVE-only 语义和对 handle 数组的接口形态
  - 2.4.2 在 PASSIVE 层构建/更新 broadphase context（基于 `FclAcquireGeometryReference` + `BuildGeometryBinding`），调用 `FclBroadphaseCoreFromBindings` 完成实际 broadphase 计算
  - 2.4.3 确保错误码与当前实现兼容，并在注释中明确 core/api 分层关系

## 3. DPC 友好上下文与结果快照模式

- [ ] 3.1 在 driver 层定义一个通用的 `FCL_DPC_DETECTION_CONTEXT` 结构：包含 snapshot 指针/拷贝、当前 transform/motion、最后一次碰撞/CCD 结果与 `Sequence` 计数
- [ ] 3.2 实现 DPC 写入协议：
  - 3.2.1 DPC 回调中先调用 Core API 完成 FCL 计算
  - 3.2.2 将结果写入 `Last*` 字段后，调用 `InterlockedIncrement64(&Sequence)` 作为写完成标记
- [ ] 3.3 实现在 PASSIVE 线程读取 DPC 结果的 helper：使用“两次读 Sequence”的快照模式保证读到完整结果
- [ ] 3.4 为未来的 ISR 读取预留接口形态（仅消费 `Sequence` + 结果快照），但在本 change 中不在 ISR 中挂接实际逻辑

## 4. 周期碰撞 / CCD 路径接入 Snapshot Core（骨架）

- [ ] 4.1 在现有 `periodic_scheduler` 使用点（如周期碰撞）上，设计一份基于 Snapshot Core 的 DPC 执行骨架：
  - 4.1.1 PASSIVE 侧负责构建 `FCL_DPC_DETECTION_CONTEXT`（包括 `FCL_GEOMETRY_SNAPSHOT` 与 motion）
  - 4.1.2 DPC 侧调用 `*CoreFromSnapshots` 完成检测，将结果写入上下文
- [ ] 4.2 暂不改变现有周期碰撞的对外 IOCTL 行为，仅在内部添加使用 Core 层的实验性路径或 feature flag

## 5. 验证与文档

- [ ] 5.1 使用现有自测（`kernel/selftest`）和 demo 验证：重构后的 PASSIVE API 行为与当前版本一致（包括状态码与诊断统计）
- [ ] 5.2 在有限场景下（例如简单 mesh + sphere）验证 Core API 在 DPC 中调用不会触发 IRQL/锁相关 Bug（可通过 Driver Verifier / KD 辅助）
- [ ] 5.3 更新 `docs/architecture.md` 与相关文档，描述新的三层结构（几何管理 / Snapshot Core / DPC 调度）及 IRQL 约束
- [ ] 5.4 运行 `openspec validate move-fcl-detection-to-dpc --strict`，确保 proposal 与任务列表满足项目规范
