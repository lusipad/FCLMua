FCL+Musa 架构说明
=================

## 模块概览
- **内存子系统**：`pool_allocator.cpp` 提供 NonPagedPool RAII 及统计，所有 STL/Eigen/libccd 分配经由此层。
- **几何管理 (`geometry_manager.cpp`)**：负责 Sphere/OBB/Mesh 创建、句柄表、引用计数、BVH 构建与更新。
- **碰撞核心 (`collision.cpp`, `narrowphase/gjk.cpp`)**：统一 Dispatch 矩阵，Sphere/OBB 特化 + Mesh fallback 至 typed GJK/EPA。
- **宽阶段 (`broadphase.cpp`)**：基于 AABB/OBBRSS 计算粗检测对，Mesh BVH 根包围体由 `FclBuildBvhModel` 提供。
- **连续碰撞 (`continuous_collision.cpp`)**：支持 InterpMotion、ScrewMotion，默认采用 Conservative Advancement，低速时退回二分法。
- **自测 (`self_test.cpp`)**：涵盖初始化、几何、碰撞、CCD、压力、Driver Verifier 等路径，结果由 `FCL_SELF_TEST_RESULT` 导出。

## 数据流
1. **几何管理**：用户通过 IOCTL 或 API 创建几何 -> 记录至 AVL 表 -> 提供快照供碰撞/距离使用。
2. **碰撞检测**：`FclCollisionDetect` 根据类型查 Dispatch -> Sphere/OBB 特化或 GJK/EPA -> 输出 `FCL_CONTACT_INFO`。
3. **连续碰撞**：构建运动描述 -> Conservative Advancement 循环计算 `FclCollisionDetect` / `FclDistanceCompute`，返回 TOI。
4. **自测/健康**：`FclRunSelfTest` 依次调用各验证场景，并记录池统计/Verifier/压力耗时等信息。

## 关键依赖
- **Musa.Runtime**：提供内核兼容的 STL/异常封装。
- **Eigen**：经 `eigen_config.h` 包装，禁用对齐/调试，供 OBBRSS、数学自测等使用。
- **libccd**：源代码嵌入 `external/libccd`，经 typed solver 适配 GJK/EPA。

## 安全/性能策略
- 全面使用 NonPagedPool，`FCL_POOL_STATS` 可实时监控。
- Driver Verifier 结果通过自测结构暴露；自测脚本可检测泄漏、性能退化、连续碰撞准确性。
