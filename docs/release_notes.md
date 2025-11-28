# 发布说明

## v0.2 – 平台抽象与 CPM 集成（2025-11-28）

### 新增特性

- **平台抽象层**：新增 `platform.h`，统一内核（R0）和用户态（R3）的 API 接口
  - 自动适配 EX_PUSH_LOCK（R0）和 SRWLOCK（R3）
  - 日志系统支持内核 DbgPrint 和用户态 fprintf
  - 通过 `FCL_MUSA_KERNEL_MODE` 宏控制编译模式

- **用户态静态库**：支持构建 `FclMusa::CoreUser` 静态库，可用于 R3 项目
  - 提供用户态 stub 函数（`FclInitialize`、`FclCleanup` 等返回 `STATUS_NOT_SUPPORTED`）
  - 兼容标准 C++ 环境，无需 WDK 即可集成核心算法

- **CPM 集成支持**：新增 `docs/cpm_integration.md` 和 `CMakeLists.txt`
  - 支持通过 CPM 包管理器引入 FclMusa 源码
  - 可选择性构建 R0/R3 静态库
  - 自动检测 WDK 环境并适配编译参数

### 改进

- **代码结构优化**：所有头文件从直接 `#include <ntddk.h>` 改为 `#include "fclmusa/platform.h"`
- **geometry_manager.cpp**：重构几何管理模块，优化引用计数和资源管理（+352 行）
- **pool_allocator.cpp**：改进内存池分配器，增强统计和调试功能（+252 行）

### 文档更新

- `docs/architecture.md`：新增平台抽象章节
- `docs/file_structure.md`：添加 `platform.h` 详细说明
- `docs/index.md`：补充 CPM 集成指南链接
- `README.md`：添加特性列表，修正文档索引链接

---

## v0.1 – 内核移植原型（2025-11-12）

### 亮点

- **几何支持**：Sphere / OBB / Mesh，内置 BVHModel + OBBRSS，支持增量更新
- **碰撞**：静态碰撞（GJK/EPA + 窄相调度）、宽相（DynamicAABBTree），Demo IOCTL 支持球体快速验证
- **连续碰撞（CCD）**：InterpMotion + ScrewMotion + Conservative Advancement
- **距离查询**：输出最近点、距离值，可用于安全监控或规划
- **自检框架**：`IOCTL_FCL_SELF_TEST` 覆盖初始化、几何创建/销毁、碰撞/CCD/压力/Verifier/泄漏等
- **运行时**：整合 Musa.Runtime，自带 NonPagedPool 分配器与 STL 适配器，提供最小 CRT stub

### 构建与部署

1. `tools/manual_build.cmd`（或 `build_driver.cmd`）
2. 导入 `FclMusaTestCert.cer`（若启用测试签名）
3. `sc create` + `sc start`
4. `tools\fcl-self-test.ps1` 验证驱动状态

### 已知限制

- 尚未提供 Release 构建/正式签名流程
- WinDbg/Hyper-V 调试流程需参考 `docs/vm_debug_setup.md` 手动配置
- 未针对多核/高并发场景进行长时间压力测试

### 后续计划

- 完成 WinDbg/KD 自动化配置脚本
- 进一步裁剪上游 FCL，减少内存占用
- 扩展 R3 Demo（Mesh 编辑、场景脚本化）
- 如需发布版本，请补充 Release 构建流水线与签名材料
