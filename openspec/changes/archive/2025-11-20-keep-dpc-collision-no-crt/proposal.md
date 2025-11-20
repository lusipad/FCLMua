## Why

- Debug 版本在 `FclPeriodicCollisionDpc` 内调用 `FclCollisionCoreFromSnapshots` 时触发 CRT 调试分配 / `_chkstk` 等 pageable 路径，DISPATCH_LEVEL 下引发蓝屏 (`IRQL_NOT_LESS_OR_EQUAL`，dump 位于 `D:\dump`)。
- 现有 PASSIVE worker 旁路会改变实时性；需求必须维持 DPC 线程执行，只能让 DPC 避免触达 debug CRT / pageable 代码。
- 目标：最小改动（保持 IOCTL / 结构体不变）下让 DPC 路径使用 NonPaged 预分配与内建分配器，消除 CRT 依赖。

## What Changes

1. **编译期开关：禁用 DPC 下 debug CRT**
   - 新增 `FCL_MUSA_DPC_NO_DEBUG_CRT`（Debug 默认开启，Release 可关闭），用于 kernel/core 构建。
   - 定义 `_CRT_NO_DBG_MEMORY_ALLOC`、禁用 `_Crt*` 报告，避免 debug CRT 进入 pageable 代码；必要时将 DPC 相关目标链接至 release CRT 或使用 `libcntpr` 替代。

2. **DPC 安全分配器与预分配工作区**
   - 在 `kernel/core/include` 提供 `FclDpcNonPagedAllocator`（基于 NonPagedPoolNx tag + lookaside），暴露 `New/Delete` 包装，在 `IRQL > PASSIVE` 时绕过 CRT。
   - `FclPeriodicCollisionDpc` 改用预分配的 `FCL_CONTACT_INFO`/`FCL_COLLISION_RESULT`/临时向量缓冲（在 `HandleStartPeriodicCollisionDpc` PASSIVE 阶段分配并缓存指针）；DPC 内不再调用通用 new/delete。
   - 关键数据保持原有 `g_PeriodicCollisionState`，只新增 `Scratch` 子结构存放非分页缓冲，并在 Stop 时释放。

3. **防页化与诊断**
   - 使用 `#pragma alloc_text` / 注解确保碰撞核心路径（DPC 入口与包装）放置于 `.text` 非分页段。
   - 增加 `FCL_LOG_INFO` 日志在 Start/Stop 标记“DPC no-CRT mode”，便于 WinDbg/Verifier 观察。

## Impact

- **不改协议**：IOCTL 与结果结构不变，用户态透明。
- **DPC 仍执行业务**：保持实时性，将分配与调试 CRT 彻底移出 DPC。
- **代码改动面小**：增设编译宏 + allocator 包装 + 预分配缓冲字段，不触碰核心算法。

## Validation

1. Debug 构建：启动/停止周期碰撞，确认日志显示 “DPC no-CRT mode”，DPC 路径无蓝屏。
2. Release 构建：行为与现状一致（可关闭宏回退现行 CRT 链接），跑冒烟验证。
3. `openspec validate keep-dpc-collision-no-crt --strict`。
