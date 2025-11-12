# Project Context

## Purpose
FCL+Musa 是一个 Windows 驱动开发项目，旨在提供内核级系统功能支持。

## Tech Stack
- C/C++（驱动开发）
- Windows Driver Kit (WDK)
- Visual Studio / MSBuild
- Windows 内核 API

## Project Conventions

### Code Style
- 遵循 Windows 驱动开发最佳实践
- 使用匈牙利命名法（Hungarian Notation）适用于驱动代码
- 保持代码注释清晰，特别是关键的内核操作
- IRQL 级别敏感的代码需明确标注

### Architecture Patterns
- 采用 WDF（Windows Driver Framework）或 WDM 模型
- 分层架构：用户态接口 ↔ 驱动核心 ↔ 硬件抽象层
- 使用标准 Windows 驱动 IRP 处理模式
- 确保线程安全和同步机制

### Testing Strategy
- 内核调试器（WinDbg）进行驱动调试
- Driver Verifier 进行内存和资源泄漏检测
- 单元测试覆盖用户态组件
- 在虚拟机或测试机器上进行集成测试（避免破坏开发环境）

### Git Workflow
- 主分支（main/master）保持稳定可发布状态
- 功能分支（feature/*）用于新功能开发
- 修复分支（fix/*）用于 bug 修复
- 提交信息使用 Conventional Commits 格式

## Domain Context
- **内核模式开发**：代码运行在 Ring 0，需要极高的稳定性和安全性
- **IRQL 级别**：注意不同 IRQL 级别下的 API 限制
- **内存管理**：使用非分页池（NonPagedPool）用于 DISPATCH_LEVEL 或更高级别
- **驱动签名**：发布版本需要 Microsoft 签名或测试签名

## Important Constraints
- 必须兼容 Windows 10/11 内核
- 不能在驱动中使用标准 C 运行时库（CRT）的某些函数
- 避免在高 IRQL 级别执行耗时操作
- 内存分配失败必须妥善处理，不能导致系统崩溃
- 符合 Microsoft 驱动质量标准（WHQL）

## External Dependencies
- Windows SDK
- Windows Driver Kit (WDK)
- 可能依赖的硬件抽象层（HAL）
- 第三方库需要是内核兼容版本
