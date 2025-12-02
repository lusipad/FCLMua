# Kernel 单元测试扩展计划

## 1. 背景
- `kernel/tests/src/self_test.cpp` 当前只覆盖球体、网格、宽阶段、CCD 与 Eigen/Musa.Runtime 冒烟路径，缺少对核心模块的行为验证。
- README roadmap 中的“性能优化 / 更多几何体类型支持”都依赖稳定的内核测试网，需先补齐单元测试。
- CI 要求 `pwsh tools/scripts/run_all_tests.ps1` 全量通过后才能提 PR，因此需要结构化的测试扩展计划。

## 2. 目标
1. 为内核各模块补充可重复的单元/集成测试，覆盖关键 API、异常路径与内存统计。
2. 将新增测试入口统一暴露在 `FclRunSelfTest` / `FclRunSelfTestScenario` 中，便于 IOCTL 和脚本复用。
3. 更新工具链（脚本、文档）保证开发者在本地与 CI 能一键运行全部内核测试。
4. 为后续“更多几何体类型支持”“性能优化”等任务提供可量化的回归基线。

## 3. 范围
- **包含**：`kernel/core`（memory、geometry、collision、distance、broadphase、upstream 桥接、driver_state）、`kernel/driver/src`（device_control、周期 DPC、IOCTL 编解码）、`kernel/tests` 基础设施与脚本。
- **排除**：用户态 `samples/`、R3 SDK 集成、外部依赖（Eigen/libccd）本身的测试，只通过 stub/bridge 接口验证。

## 4. 实施阶段

### 阶段 0：测试基础设施
- 在 `kernel/tests/include` 添加断言/日志辅助（如 `FCL_EXPECT_*`），统一处理 NTSTATUS。
- 调整 `FclRunSelfTestScenario`/`FclRunSelfTest`，增设新的 Scenario ID，并确保 `FCL_SELF_TEST_SCENARIO_RESULT` 填充 pool stats。
- 更新 `tools/scripts/run_all_tests.ps1`、`tools/fcl-self-test.ps1` 自动跑新增场景。

### 阶段 1：内存与运行时基础
- **模块**：`memory/pool_allocator.cpp`、`runtime/*`.
- **测试点**：
  - Initialize/Shutdown/Enable 路径、统计归零、IRQL 限制。
  - Allocate/Reallocate/Free 的 size/Tag 记录与越界/溢出处理。
  - Musa.Runtime STL/tls/new/delete 行为、异常捕获。
- **交付物**：`kernel/tests/src/memory_pool_tests.cpp`、`kernel/tests/src/runtime/runtime_tests.cpp` 及对应头文件。

### 阶段 2：几何与碰撞核心
- **模块**：`geometry_manager.cpp`、`bvh_model.cpp`、`collision/*.cpp`、`distance/distance.cpp`、`broadphase/broadphase.cpp`、`upstream/*`.
- **测试点**：
  - 几何创建/销毁/引用计数、非法描述符、BVH 重新构建。
  - Sphere/OBB/Mesh/Capsule（已实现）等组合的碰撞/距离结果，矩阵正交校验失败路径。
  - 连续碰撞参数（容差、迭代上限）、宽阶段配对上限与排序。
  - upstream 桥接异常翻译、libccd 内存钩子调用。
- **交付物**：新增 `*_tests.cpp` 文件、静态数据/夹具，并在 Scenario 中细分（例：SCENARIO_GEOMETRY, SCENARIO_COLLISION_MATRIX）。

### 阶段 3：驱动封装与 IOCTL
- **模块**：`kernel/core/src/driver_state.cpp`、`kernel/driver/src/device_control.cpp`.
- **测试点**：
  - FclInitialize/FclCleanup 的幂等与失败回滚、诊断计时器统计。
  - 周期碰撞状态机：启停流程、DPC 预算、Snapshot 协议。
  - IOCTL 输入输出边界、缓冲区不足、对齐要求、错误码。
- **交付物**：面向驱动的测试入口（受 `FCLMUSA_BUILD_DRIVER` 控制），并确保自检脚本可在设备上触发。

### 阶段 4：回归与持续化
- 将关键用例提升为 `tests/` 回归脚本，确保 SDK 层也能感知。
- 在 README/Testing 文档中补充运行方法、常见排查指引。
- 评估是否引入覆盖率工具或统计（如根据自检结果记录模块通过率）。

## 5. 排期建议（可按 Sprint/周划分）
| 周期 | 任务 | 里程碑 |
|------|------|--------|
| Week 1 | 完成阶段 0 基建 + 阶段 1 实施 | 新 Scenario 在本地可跑通 |
| Week 2 | 阶段 2（几何/碰撞） | 核心数学用例全部落地 |
| Week 3 | 阶段 3（驱动/IOCTL） | 自检脚本扩展，CI 执行 |
| Week 4 | 阶段 4（回归/文档） | 测试指南更新，形成长效机制 |

## 6. 风险与对策
- **驱动依赖 WDK/内核环境**：测试主要通过自检入口执行，避免额外内核注入；必要时在文档明确运行环境。
- **新增 Scenario 增加运行时间**：按模块拆分，可在脚本中提供 `-Filter`，默认跑关键路径。
- **外部资源占用（NonPagedPool）**：测试间严格使用 `FCL_POOL_STATS` 对比，必要时串行执行。
- **与未来几何体特性冲突**：保持测试夹具数据可扩展，避免写死几何类型枚举。

## 7. 验证与度量
- 每阶段结束需运行 `pwsh tools/scripts/run_all_tests.ps1` 与 `tools/fcl-self-test.ps1`，记录 Scenario 结果。
- 在 PR 模板/CI 日志中附上新增 Scenario 的通过截图或日志片段。
- 使用 `FCL_POOL_STATS` 追踪池内存差值，确保测试不会引入泄漏。

## 8. 交付要求
1. 所有新增测试文件遵循 C++17、K&R、大写宏等仓库规范，并在对应目录添加 Doxygen 说明。
2. 相关文档（README/testing/usage）同步更新测试运行方式。
3. 在后续“更多几何体类型支持”任务启动前，确保阶段 1-3 已合入主干并在 CI 中默认执行。

