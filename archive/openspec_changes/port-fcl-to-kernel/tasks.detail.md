# 任务分解（Phase 0/1 最小闭环）

目标：以最小变更实现“可加载、可调用、可验证”的内核态 FCL 骨干链路，支撑 ACCEPTANCE.md 的 A–G 条款。

## P0. 驱动骨架与观测
- [x] P0.1 建立驱动入口/卸载、日志设施、Pool Tag `"FCL "`
- [x] P0.2 暴露 `IOCTL_FCL_PING`（返回版本、初始化状态、最近错误、内存统计）
- [x] P0.3 `FclInitialize()`/`FclCleanup()` 空实现打通，状态机与重入保护

验收：ACCEPTANCE A、D（部分）、E（异常转译框架）

## P1. 分配器与数学基线
- [x] P1.1 自定义 NonPagedPool 分配/释放（RAII 包装，统计计数）
- [x] P1.2 Eigen 配置：禁对齐、替换分配器、禁静态断言与静态初始化依赖
- [x] P1.3 Eigen 自测：向量/矩阵加减乘、转置、范数

验收：ACCEPTANCE F

## P2. 几何体与资源管理
- [x] P2.1 Sphere/OBB/Mesh 最小数据结构与句柄表
- [x] P2.2 Create/Destroy API：参数校验、错误码、无泄漏
- [x] P2.3 辅助：句柄二次销毁与资源不足注入测试

验收：ACCEPTANCE B

## P3. 基础碰撞检测（静态）
- [x] P3.1 实现 Sphere-Sphere 判定（几何闭式或最小 GJK）
- [x] P3.2 正/反用例与边界接触用例
- [x] P3.3 结果布尔化与错误码转译

验收：ACCEPTANCE C

## P4. IOCTL 自检串联
- [x] P4.1 自检 IOCTL 触发：初始化 → 几何创建 → 碰撞检测 → 释放 → 清理
- [x] P4.2 结果与统计汇总（错误码、内存统计、版本）
- [x] P4.3 脚本化执行（WinDbg/CI 步骤）

验收：ACCEPTANCE D、G

## P5. 稳定性与异常策略
- [x] P5.1 边界错误与异常路径覆盖：无越界、无泄漏
- [x] P5.2 NTSTATUS 一致性与日志核对

验收：ACCEPTANCE E、A（泄漏为 0）

——

与 `tasks.md` 的映射：
- P0 ≈ 1.4/1.5/7.1（基础设施）
- P1 ≈ 3.x/4.x（分配器与数学内核）
- P2 ≈ 6.1/6.6（几何与 API 边界）
- P3 ≈ 6.2（最小碰撞）
- P4 ≈ 7.2/7.6/8.x（可观测与自动化）
- P5 ≈ 8.x/9.x（稳定性与质量门）

## P6. FCL 核心拓展（当前新增范围）
- [x] P6.1 BVHModel：移植 `fcl::BVHModel` 网格结构与 NonPagedPool 分配封装，支持 Mesh 构建/更新。
- [x] P6.2 OBBRSS：实现 `fcl::OBBRSS` 组合包围体与 AABB/BV 层级构建，供 Broadphase/窄相复用。
- [x] P6.3 collision 核心：接入 FCL `collision` 调度（CollisionObject、Dispatch 表），合并现有 Sphere/OBB 实现，确保 Contact 输出统一。
- [x] P6.4 libccd GJK/EPA：保留既有内核适配，同时拓展到 FCL 调度（typed solvers + ContactPoint）。
- [x] P6.5 自检扩展：新增 Mesh-Mesh、OBBRSS 场景，并在 IOCTL 输出 FCL 路径结果。

## P7. 连续碰撞与运动模型
- [x] P7.1 InterpMotion：移植线性插值运动模型，提供姿态/位移插值 API。
- [x] P7.2 ScrewMotion：移植螺旋运动模型，支持旋转/平移耦合。
- [x] P7.3 continuous_collision：接入 FCL 的 conservative advancement、TimeOfContact 计算，并暴露驱动 API。
- [x] P7.4 验证：Sphere/OBB/Mesh 连续碰撞用例、长时间运行测试。

## P8. Eigen/LibCCD 依赖维护
- [x] P8.1 Eigen 核心：将依赖范围限制在 `Core`/`Geometry`（Matrix3、Vector3、Isometry、AngleAxis 等），其余持续裁剪。
- [x] P8.2 Eigen 运行准入：更新自检确保所需 API 可用，新增 Isometry/AngleAxis 场景。
- [x] P8.3 libccd：保持 GJK/EPA 全量源文件，定期同步 upstream patch 并验证内核适配。

注：后续阶段优先保障上述模块的完成度，再进入 CCD 高阶能力或 TaylorModel 相关工作。











