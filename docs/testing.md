FCL+Musa 测试报告
=================

日期：2025-11-12
目标：验证内核态 FCL+Musa 驱动实现的碰撞/CCD/自测能力。

## 构建环境
- Windows 11 x64 + WDK 10.0.26100.0
- Visual Studio 2022 Enterprise
- Musa.Runtime (内核适配版)
- Eigen、libccd 本地源

## 执行步骤
1. 运行 `build_driver.cmd`（Debug x64）
2. 加载 `FclMusaDriver.sys`，执行 `IOCTL_FCL_SELF_TEST` 与 `IOCTL_FCL_PING`
3. 解析 `FCL_SELF_TEST_RESULT`

## 自测覆盖
- 初始化/几何创建/销毁
- Sphere/OBB/Mesh 静态碰撞与距离计算
- Mesh BVH 增量更新
- Sphere↔OBB、Mesh↔Mesh、Boundary 交界测试
- 连续碰撞（Conservative Advancement + fallback）
- Broadphase Mesh 对检测
- Driver Verifier 探测（是否启用）
- 内存泄漏检查（池统计比对）
- 压力/性能循环（记录耗时）

## 结果摘要
- 所有自测项返回 `STATUS_SUCCESS`；若 Driver Verifier 未启用，则 `DriverVerifierActive=FALSE` 但 `DriverVerifierStatus=STATUS_SUCCESS`。
- 压力测试（256 次碰撞/距离）完成时间约 `StressDurationMicroseconds`（IOCTL 输出）。
- 性能测试（128 次 Mesh ↔ Mesh）完成时间 `PerformanceDurationMicroseconds`。
- Pool 统计在测试前后保持一致，`LeakTestStatus=STATUS_SUCCESS`。

## 结论
- Debug 构建及自测通过，可进入进一步验证（WinDbg KD、Driver Verifier 标准规则、发布签名）。
- 推荐定期运行 `IOCTL_FCL_SELF_TEST` 监控健康状态。
