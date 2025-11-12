# 提案：将 FCL 移植到 Windows 内核驱动

## Why（为什么）

需要在 Windows 内核模式下提供高性能的 3D 碰撞检测能力。FCL (Flexible Collision Library) 是一个成熟的 C++ 碰撞检测库，但原本设计用于用户态。通过使用 Musa.Runtime 作为 STL 替代，可以将 FCL 移植到内核驱动中，为内核态应用提供碰撞检测功能。

## What Changes（改变内容）

### 核心变更
- 集成 Musa.Runtime 作为内核态 C++ STL 运行时
- 移植 FCL 核心碰撞检测功能到内核模式
- 适配 FCL 的依赖库（Eigen、libccd）到内核环境
- 实现内核兼容的内存管理和资源分配
- 提供内核态 API 接口供其他驱动模块调用

### 新增功能
- 内核态 3D 碰撞检测（Collision Detection）
- 内核态距离计算（Distance Computation）
- 内核态容差验证（Tolerance Verification）
- 可选的连续碰撞检测（Continuous Collision Detection）

#### 模块覆盖（当前需求）
- **BVHModel + OBBRSS**：移植 FCL 的 BVH 模型与组合包围体，支撑 Mesh/组合碰撞加速。
- **collision**：引入 FCL 常规碰撞调度与 Contact 生成，覆盖 Sphere/Box/Mesh 等组合。
- **continuous_collision**：保留 Conservative Advancement 流程，匹配 FCL 连续碰撞管线。
- **InterpMotion / ScrewMotion**：实现运动模型，供连续碰撞与路径预测使用。
- **Eigen 依赖**：仅保留 Core/Geometry（Matrix3、Vector3、Isometry、AngleAxis 等核心 API），其余彻底裁剪。
- **libccd 依赖**：完整携带 GJK/EPA，实现静态/连续双路径的凸体支持。

### 技术约束遵循
- 遵守 IRQL 级别限制
- 使用非分页内存池（NonPagedPool）
- 避免浮点运算（除非在 PASSIVE_LEVEL）
- 处理异常和错误而不导致 BSOD

## Impact（影响分析）

### 新增规范
- **collision-detection**：定义内核态碰撞检测功能规范

### 影响的代码
- 新增模块：
  - `src/collision/` - 碰撞检测核心
  - `src/runtime/` - Musa.Runtime 集成
  - `src/math/` - 内核态数学库适配（Eigen）
  - `src/geometry/` - 几何对象定义

### 外部依赖
- Musa.Runtime (https://github.com/MiroKaku/Musa.Runtime)
- FCL 源码（需要修改以适配内核）
- Eigen 库（需要内核兼容版本）
- libccd（可能需要重新实现或移植）

### 风险
- **性能风险**：内核态浮点运算性能可能不如用户态
- **稳定性风险**：内存泄漏或异常处理不当可能导致系统崩溃
- **兼容性风险**：Musa.Runtime 可能不支持所有 FCL 依赖的 STL 功能
- **维护风险**：FCL 上游更新需要手动合并和适配

### 测试策略
- 在虚拟机中进行开发和测试
- 使用 Driver Verifier 检测内存问题
- 编写单元测试覆盖核心算法
- 性能基准测试（与用户态版本对比）

## Timeline（修订后的预估时间）

**⚠️ 警告：本项目为高风险、高复杂度工程**

基于深度代码分析，实际工作量远超初步评估：
- **总代码规模**：27,714 行（静态碰撞 10,000 + CCD 17,714）
- **需要修改的文件**：150+ 个
- **预计工作量**：36-49 周（9-12 个月）
- **成功率估计**：< 30%

### 分阶段时间线

#### 阶段 0：风险验证和决策（4 周）
- 第 1-2 周：Musa.Runtime 完整功能验证
- 第 3-4 周：数学函数库可行性验证（关键里程碑）
- **决策点 1**：如果数学库无法实现 → 考虑降级方案

#### 阶段 1：基础设施（6-8 周）
- 第 5-6 周：环境搭建和 Musa.Runtime 集成
- 第 7-10 周：内核态数学函数库实现（sin, cos, exp, log, sqrt）
- 第 11-12 周：内存管理和资源追踪系统
- **决策点 2**：验证基础设施稳定性

#### 阶段 2：Eigen 和 libccd 适配（8-12 周）
- 第 13-16 周：Eigen 核心适配（禁用 SIMD、重定向内存）
- 第 17-20 周：Eigen 高级功能测试（矩阵运算、变换）
- 第 21-24 周：libccd 移植和测试
- **决策点 3**：验证 Eigen 性能是否可接受

#### 阶段 3：静态碰撞检测（8-10 周）
- 第 25-28 周：几何对象和 BVH 结构
- 第 29-32 周：碰撞检测核心算法
- 第 33-34 周：静态碰撞 API 和测试
- **里程碑**：静态碰撞检测可用

#### 阶段 4：区间算术和泰勒模型（10-14 周）
- 第 35-38 周：区间算术核心（舍入控制）
- 第 39-42 周：区间数学函数（sin, cos, exp, log）
- 第 43-46 周：泰勒模型实现（TaylorModel, TaylorVector, TaylorMatrix）
- 第 47-48 周：泰勒模型验证和测试
- **决策点 4**：泰勒模型精度是否满足要求

#### 阶段 5：运动模型（4-6 周）
- 第 49-50 周：TranslationMotion（平移）
- 第 51-52 周：InterpMotion（插值）
- 第 53-54 周：ScrewMotion（螺旋）
- 第 55-56 周（可选）：SplineMotion（样条）

#### 阶段 6：连续碰撞检测（6-8 周）
- 第 57-60 周：保守推进算法核心
- 第 61-64 周：CCD 遍历节点和 BVH 集成

#### 阶段 7：I/O 操作移除（4-6 周）
- 第 65-68 周：移除所有 std::cout/cerr（80+ 文件）
- 第 69-70 周：实现内核日志系统

#### 阶段 8：集成测试和优化（6-8 周）
- 第 71-74 周：功能测试和边界情况
- 第 75-76 周：性能优化和内存优化
- 第 77-78 周：Driver Verifier 和稳定性测试

#### 阶段 9：文档和交付（2-3 周）
- 第 79-81 周：文档、代码审查和发布准备

**总计：36-49 周（预留缓冲）**

## Alternatives Considered（备选方案）

1. **从头实现碰撞检测算法**
   - 优点：完全控制，无依赖
   - 缺点：开发时间长，可能不如 FCL 成熟

2. **使用用户态服务 + IPC**
   - 优点：无需移植，稳定性高
   - 缺点：性能开销大，不适合实时场景

3. **使用其他内核兼容的碰撞检测库**
   - 优点：可能已经适配内核
   - 缺点：未找到成熟的内核态碰撞检测库

## Success Criteria（成功标准）

- ✅ 驱动可以在 Windows 10/11 上加载和卸载
- ✅ 核心碰撞检测功能正常工作
- ✅ 通过 Driver Verifier 所有测试
- ✅ 无内存泄漏和资源泄漏
- ✅ 性能满足实时需求（待定义具体指标）
- ✅ 符合 WHQL 质量标准
