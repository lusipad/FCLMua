## ADDED Requirements

### Requirement: Periodic collision DPC SHALL avoid debug CRT and pageable paths
驱动 DPC 周期碰撞路径 SHALL 仅依赖非分页内存与 DPC-safe 分配器，避免触发 debug CRT / pageable 代码导致蓝屏。

#### Scenario: Debug build runs collision in DPC safely
- **GIVEN** Debug 构建且启用 `FCL_MUSA_DPC_NO_DEBUG_CRT`
- **WHEN** 发起 `IOCTL_FCL_START_PERIODIC_COLLISION`
- **THEN** 驱动 SHALL 在 DPC 中仅使用预分配 NonPaged 缓冲执行碰撞计算，严禁调用 CRT new/delete 或 `_Crt*`，并输出 “DPC no-CRT mode” 日志
- **AND** 停止时释放 Scratch 缓冲且不触发任何 pageable 调用

#### Scenario: Release build keeps current DPC behavior
- **GIVEN** Release 构建或关闭 `FCL_MUSA_DPC_NO_DEBUG_CRT`
- **WHEN** 启动/停止周期碰撞
- **THEN** 驱动 SHALL 保持现有 DPC 路径和性能，且不受新的分支影响。
