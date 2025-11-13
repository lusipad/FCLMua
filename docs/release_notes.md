FCL+Musa 发布说明
=================

版本：v0.1 (内核移植原型)
日期：2025-11-12

主要特性
--------
- BVHModel/OBBRSS Mesh 支持与 NonPagedPool 封装
- Sphere/OBB/Mesh 碰撞、距离与连续碰撞 (Conservative Advancement)
- Mesh 几何增量更新 API (`FclUpdateMeshGeometry`)
- 自测覆盖几何/碰撞/CCD/压力/Verifier 状态
- IOCTL：Ping、SelfTest、Collision、Distance

构建与部署
----------
1. `tools/manual_build.cmd`（或 `build_driver.cmd`，Debug x64）
2. 使用 sc 或 VS 驱动部署加载 `FclMusaDriver.sys`
3. 建议在测试模式或自签证书下运行

已知问题
--------
- WinDbg 调试链路尚未配置（参见 docs/known_issues.md）
- 尚无 Release 构建/签名

后续计划
--------
- 完成 WinDbg/Driver Verifier 标准流程
- 编写更详尽的 API/示例/架构文档
- 启动 WHQL 或内部签名流程
