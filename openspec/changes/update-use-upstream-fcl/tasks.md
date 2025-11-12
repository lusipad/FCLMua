## 1. 规范与范围
- [x] 1.1 明确 `collision-detection` 能力 100% 复用 upstream FCL 算法
- [x] 1.2 限定 driver 仅保留 allocator/logging/NTSTATUS 等最小化 hook
- [x] 1.3 约定所需的等价性验证标准，确保 R0 与 upstream 结果一致

## 2. 方案交付
- [x] 2.1 在 `design.md` 中完成 upstream ↔ R0 适配层设计
- [x] 2.2 在架构/README/部署文档中记录 upstream FCL 依赖与约束

## 3. 实施与验证（审批通过后执行）
- [ ] 3.1 完成 `upstream_bridge` 连续碰撞实现，调用 upstream `fcl::continuousCollide`
- [ ] 3.2 清理遗留 `narrowphase/gjk.cpp` 等旧实现并更新所有引用
- [ ] 3.3 修订构建脚本，确保构建链固定使用 commit `5f7776e2101b8ec95d5054d732684d00dac45e3d`
- [ ] 3.4 增加自动化回归测试，对 Driver IOCTL 输出与 upstream FCL（碰撞/距离/CCD）逐场景比对
- [ ] 3.5 运行 `openspec validate update-use-upstream-fcl --strict`，验证通过后发起审批
