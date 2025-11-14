## 1. 需求分析与基线构建
- [ ] 1.1 梳理 FCL CCD 路径（continuous_collision / InterpMotion / TranslationMotion 等），列出内核相关调用链与关键函数
- [ ] 1.2 在 R3 环境下构造一批典型 CCD 场景（球/盒/mesh，平移+旋转），记录 upstream FCL 的 TOI / 是否相交 / 接触姿态，作为参考基线
- [ ] 1.3 在当前 R0 实现上重放相同场景（在 KD 附着环境下），确认栈溢出问题的可复现性与触发条件（几何规模、角速度等）

## 2. Kernel CCD Profile 设计
- [ ] 2.1 设计 `FCL_MUSA_KERNEL_MODE` 编译宏的使用范围（仅限 FclCore 依赖的 fcl-source 模块），保证 R3 工具仍使用 upstream 默认实现
- [ ] 2.2 明确 Kernel profile 下 CCD 支持的组合：
  - motion 类型：`CCDM_TRANS` + `CCDM_LINEAR`
  - solver 类型：`CCDC_CONSERVATIVE_ADVANCEMENT`（其他组合在内核下返回 NotSupported）
- [ ] 2.3 为以上策略补充高层设计文档（可放在本 change 的 design.md），说明内核 CCD 与 R3 CCD 的行为对齐原则和差异

## 3. 内核友好版 InterpMotion 实现
- [ ] 3.1 为 `fcl::InterpMotion` 设计内核友好实现方案：
  - 保持 class 接口与成员布局兼容
  - 改写 `computeVelocity` 等关键路径，降低 Eigen 表达式复杂度和局部变量规模
- [ ] 3.2 在 R3 下实现并对比：
  - 对随机姿态对（R1/T1, R2/T2）计算 linear_vel / angular_vel / angular_axis
  - 对 t∈{0,0.25,0.5,0.75,1} 比较 R(t)/T(t)，设定可接受误差阈值
- [ ] 3.3 在 R0（KD 附着）下评估栈使用情况（关键帧栈深度），确保留有足够安全裕度

## 4. continuousCollide 内核路径收紧
- [ ] 4.1 在 Kernel profile 下显式限制 continuousCollide 支持的 CCD 组合（见任务 2.2），未支持组合直接返回错误码而不是进入未减栈路径
- [ ] 4.2 针对“纯平移 CCD”路径，优先使用 TranslationMotion + Conservative Advancement，并对 mesh/mesh 场景做额外验证
- [ ] 4.3 针对“带旋转 CCD”路径，使用内核友好 InterpMotion + Conservative Advancement，并在 R0/R3 之间做场景对比与栈稳定性测试

## 5. 自测与压力测试
- [ ] 5.1 在 `kernel/selftest` 中增加 CCD 自测场景（球/盒/mesh 的典型平移+旋转用例），仅在 Debug 下默认启用
- [ ] 5.2 使用 Driver Verifier + KD，对 CCD IOCTL 做多线程、长时间压力测试，验证无蓝屏/无资源泄漏
- [ ] 5.3 对比 R3 vs R0 CCD 行为（TOI/接触信息），确认落在可接受误差范围内；若出现差异，更新文档说明数值特性或回调算法实现

## 6. 文档与 OpenSpec 更新
- [ ] 6.1 在 `openspec/changes/add-kernel-ccd-support/specs/kernel-ccd/spec.md` 中补充/新增内核 CCD 能力的要求与场景
- [ ] 6.2 更新 `docs/architecture.md`、`docs/api.md`，说明：
  - R0 CCD 的支持范围、使用限制
  - 与 R3 CCD 的关系（哪些场景推荐在 R3 做规划）
- [ ] 6.3 更新 `docs/testing.md` / `docs/demo.md`：
  - 增加 CCD 相关测试步骤与 demo 用法
  - 标明 Debug 自测开关及如何在测试环境中开启/关闭 CCD 自测

