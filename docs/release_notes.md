# 发布说明

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
