## Why

当前内核版 FclCore 已经提供了 `FclContinuousCollision` + `IOCTL_FCL_CONVEX_CCD` 能力，但：

- 使用 upstream FCL 的 `InterpMotion` + Eigen 默认实现时，在内核栈上会产生较大的栈帧；
- 在复杂 CCD 场景（带旋转）下，R0 可能出现栈溢出导致 BugCheck（当前 dump 已验证这一点）；
- CCD 的“可用性”和“发布级稳定性”尚未达到平台正式能力的标准。

我们需要一个“内核友好”的 FCL CCD 方案，既保留带旋转的完整功能，又在 R0 下具备可预测的栈占用和稳定性，能够作为正式发布版本的一部分对外使用。

## What Changes

- 在 fcl-source 内新增一个 **Kernel CCD Profile**：
  - 通过编译宏（如 `FCL_MUSA_KERNEL_MODE`）启用；
  - 仅在内核目标（FclMusaCoreLib）启用，R3 工具继续使用 upstream 默认实现。
- 为 `fcl::InterpMotion` / `continuousCollide` 提供一套 **内核友好实现**：
  - 保持 API / 类型接口不变；
  - 调整 `InterpMotion::computeVelocity` 等关键路径的实现，降低栈占用；
  - 明确在 Kernel profile 下支持的 CCD 组合（motion 类型 / solver 类型）；
  - 为不支持的组合返回“Not Supported”，而不是走到未减栈路径。
- 建立一套 **R3 vs R0 CCD 行为基线**：
  - 在 R3 使用 upstream FCL 作“参考实现”；
  - 在 R0 使用 Kernel profile 下的 FclCore CCD 作“目标实现”；
  - 对典型 CCD 场景（球 / 盒 / mesh，平移 + 旋转）进行数值对比，限定 TOI / 接触误差。
- 增强 CCD 相关的测试与文档：
  - 在 kernel/selftest 中增加 CCD 场景（仅在 Debug 下跑，可选 Release 开关）；
  - 为 CCD IOCTL/内核 API 补充架构说明、使用约束与场景示例；
  - 将“带旋转 CCD in kernel”写入 OpenSpec 作为正式能力条目。

## Impact

- Affected specs:
  - 新增能力：`kernel-ccd`（内核连续碰撞检测能力）
  - 现有碰撞/几何能力说明需补充 CCD 部分
- Affected code:
  - `fcl-source/include/fcl/math/motion/interp_motion{.h,.inl}`
  - `fcl-source/include/fcl/narrowphase/continuous_collision{.h,.inl}`
  - `kernel/core/src/upstream/upstream_bridge.cpp`
  - `kernel/selftest/src/**`（新增 CCD 自测）
  - 文档：`docs/architecture.md`, `docs/api.md`, `docs/testing.md`, `docs/demo.md`

