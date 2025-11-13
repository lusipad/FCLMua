FCL+Musa 使用指南
=================

1. 安装/构建
------------
- 安装 WDK 10.0.26100.0、VS 2022（已完成）。
- 在仓库根目录运行 `tools/manual_build.cmd`（或 `build_driver.cmd`），生成 `kernel/FclMusaDriver/out/x64/Debug/FclMusaDriver.sys`。`manual_build.cmd` ��װ VsDevCmd/WDK �����Ժ��޽�����，`build_driver.cmd` ���Զ�����������ǩ��，��Ҫ�û����롣

2. 驱动加载
------------
- 使用 `sc create` 或 VS 驱动部署工具加载 `FclMusaDriver.sys`。
- 建议在测试机启用测试签名或使用自签名证书。

3. IOCTL 操作
--------------
- 设备名称：`\\.\FclMusaDriver`（示例，视 driver_entry 实际命名）。
- 主要 IOCTL：
  - `IOCTL_FCL_PING`：健康检查，返回版本/内存统计。
  - `IOCTL_FCL_SELF_TEST`：运行内置自测，输出 `FCL_SELF_TEST_RESULT`。
  - `IOCTL_FCL_QUERY_COLLISION`：执行两对象碰撞检测。
  - `IOCTL_FCL_QUERY_DISTANCE`：执行距离计算。

4. 几何与碰撞示例
------------------
```c
FCL_SPHERE_GEOMETRY_DESC sphere = {{0,0,0}, 1.0f};
FCL_GEOMETRY_HANDLE sphereHandle = {};
FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphere, &sphereHandle);

FCL_COLLISION_OBJECT_DESC obj1 = {sphereHandle, IdentityTransform()};
FCL_COLLISION_OBJECT_DESC obj2 = {sphereHandle, IdentityTransform()};
obj2.Transform.Translation.X = 1.5f;

FCL_COLLISION_QUERY_RESULT result = {};
FclCollideObjects(&obj1, &obj2, nullptr, &result);
```

5. 连续碰撞（CCD）
-----------------
```c
FCL_CONTINUOUS_COLLISION_QUERY query = {};
query.Object1 = sphereHandle;
query.Motion1 = ...; // InterpMotionInitialize
query.Object2 = sphereHandle;
query.Motion2 = ...;
query.Tolerance = 1e-4;
query.MaxIterations = 64;

FCL_CONTINUOUS_COLLISION_RESULT ccd = {};
FclContinuousCollision(&query, &ccd);
```

6. 调试建议
------------
- `IOCTL_FCL_SELF_TEST` 查看 `DriverVerifierStatus`/`LeakTestStatus` 等字段。
- 启用 WinDbg KD，建议配置 KDNET + KDNETKEY。
- 使用 Driver Verifier (`verifier.exe /standard /driver FclMusaDriver.sys`) 验证池/IRQL 行为。

7. 卸载/清理
-------------
- 停止驱动前调用 `FclCleanup()`，避免残留引用。
- 卸载 service 并删除 `.sys` 文件。

