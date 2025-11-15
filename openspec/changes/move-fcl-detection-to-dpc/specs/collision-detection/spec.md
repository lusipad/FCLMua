# Collision Detection – DPC 迁移增量规范

## ADDED Requirements

### Requirement: DPC 安全的碰撞检测核心

系统 SHALL 提供可以在 `IRQL <= DISPATCH_LEVEL` 调用的基于快照的碰撞 / 距离 / CCD Core API，这些 Core API 不得依赖几何句柄锁（如 PushLock/AVL）或分页内存。

#### Scenario: DPC 周期碰撞检测
- **WHEN** 周期性碰撞检测通过专用 IOCTL 被配置为在 DPC 中运行
- **THEN** 驱动 SHALL 在 DPC 中调用 snapshot Core API 完成碰撞 / 距离 / CCD 计算
- **AND** PASSIVE 层 `FclCollisionDetect` / `FclDistanceCompute` / `FclContinuousCollision` 的外部语义保持与变更前一致

#### Scenario: PASSIVE 包装器兼容性
- **WHEN** 现有上层 R0 代码继续调用 PASSIVE API（例如 `FclCollisionDetect`）
- **THEN** 这些 API SHALL 在 PASSIVE 层完成句柄解析 / 快照获取，再转调 snapshot Core API
- **AND** 任何失败路径和返回 NTSTATUS SHALL 与变更前的行为兼容

### Requirement: DPC 碰撞诊断统计

系统 SHALL 记录在 DPC 上下文中执行的碰撞检测时间统计，并通过 diagnostics IOCTL 暴露单独的统计字段。

#### Scenario: DPC 诊断字段
- **WHEN** 用户态调用 `IOCTL_FCL_QUERY_DIAGNOSTICS`
- **THEN** 返回的 `FCL_DIAGNOSTICS_RESPONSE` 结构 SHALL 包含 `DpcCollision` 统计字段
- **AND** `DpcCollision` 只聚合在 `IRQL == DISPATCH_LEVEL` 下执行的碰撞检测调用

#### Scenario: PASSIVE 与 DPC 统计解耦
- **WHEN** 碰撞检测在 PASSIVE 线程中执行
- **THEN** 统计 SHALL 只影响 `Collision` 聚合字段而不影响 `DpcCollision`
- **AND** 现有基于 `Collision` 字段的诊断脚本/工具无需修改即可保持可用

