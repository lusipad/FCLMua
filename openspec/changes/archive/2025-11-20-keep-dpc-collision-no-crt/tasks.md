## 1. 编译宏与 CRT 配置

- [ ] 1.1 在 common props / CMake 工程引入 `FCL_MUSA_DPC_NO_DEBUG_CRT`，Debug 默认开启。
- [ ] 1.2 `kernel/core` / `kernel/driver` 在该宏存在时定义 `_CRT_NO_DBG_MEMORY_ALLOC`，并链接 non-debug CRT（或禁用 `_Crt*`）。

## 2. DPC 安全工作区

- [ ] 2.1 在 `FCL_PERIODIC_COLLISION_STATE` 添加 `Scratch`（NonPagedPool 缓冲 + lookaside）及生命周期管理。
- [ ] 2.2 `HandleStartPeriodicCollisionDpc` 初始化 Scratch，`HandleStopPeriodicCollisionDpc` 释放。
- [ ] 2.3 `FclPeriodicCollisionDpc` 改用 Scratch 缓冲，禁止调用 CRT new/delete。
- [ ] 2.4 为 DPC 路径添加 `#pragma alloc_text` / `_IRQL_requires_max_(DISPATCH_LEVEL)` 以确认放入非分页段。

## 3. 日志与验证

- [ ] 3.1 在 Start/Stop 记录当前模式（DPC + no-CRT）。
- [ ] 3.2 Debug build 运行周期碰撞 demo，验证无蓝屏。
- [ ] 3.3 Release build 冒烟测试（DPC 正常）。
- [ ] 3.4 `openspec validate keep-dpc-collision-no-crt --strict`。
