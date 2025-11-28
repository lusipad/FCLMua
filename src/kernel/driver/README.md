# FCL+Musa Kernel Driver Skeleton

本目录提供阶段 P0/P1 所需的最小内核驱动骨架：

- `DriverEntry`/`DriverUnload`，创建 `\Device\FclMusa` 与 `\DosDevices\FclMusa`
- `IOCTL_FCL_PING`：返回版本、初始化状态、最近错误、池统计
- `IOCTL_FCL_SELF_TEST`：触发初始化→几何创建→碰撞检测→销毁的自检流程，返回分阶段 NTSTATUS、接触信息与池差值
- `IOCTL_FCL_QUERY_COLLISION`：输入两个句柄 + 变换，返回碰撞布尔值及接触信息（`FCL_COLLISION_IO_BUFFER`）
- `IOCTL_FCL_QUERY_DISTANCE`：输入两个句柄 + 变换，返回最小距离与最近点（`FCL_DISTANCE_IO_BUFFER`）
- `FclInitialize`/`FclCleanup` 状态机，带并发保护
- `FclCreateGeometry`/`FclDestroyGeometry`：支持 Sphere/OBB/Mesh 几何创建/销毁，覆盖 NonPagedPool 分配、参数校验与异常路径回滚
- `FclCollisionDetect`：支持 Sphere-Sphere、Sphere-OBB、OBB-OBB 判定，输出布尔状态、穿透深度与接触点，并强制 PASSIVE_LEVEL 执行
- `FclDistanceCompute`：支持 Sphere-Sphere、Sphere-OBB、OBB-OBB 距离查询，返回最近点及最小距离
- `FclBroadphaseDetect`：委托 upstream FCL DynamicAABBTree 宽相管理器，根据传入句柄+变换生成潜在碰撞对
- GJK 窄相：静态集成 libccd (v2.1) 的 GJK/EPA 流程，使用内核内存包装与 NonPagedPool 支持，覆盖 Sphere/OBB/Mesh 等凸体的兜底组合
- Eigen 适配：`fclmusa/math/eigen_config.h` 自动检测 `<Eigen/Core>` 并禁用对齐/向量化，详见 `docs/eigen_adaptation.md`
- NonPagedPool RAII 分配器与全局 `new/delete` 覆盖，带池统计
- Musa.Runtime/STL 自检与 Eigen 自检入口（缺失 Eigen 时自动降级为 `STATUS_NOT_SUPPORTED` 提示）

## 工程拆分

- src/kernel/FclMusaDriver/：VS 解决方案与唯一的 `.vcxproj`，直接编译 `src/kernel/driver/src` 下的所有 Ring0 源文件并生成 `FclMusaDriver.sys`。
- src/kernel/driver/：当前目录，提供 `include/` 与 `src/` 树，涵盖碰撞/距离/宽相/内存/上游桥接以及自检脚本，驱动无需额外静态库即可使用。


## 目录结构

- `src/upstream/geometry_bridge.*`：封装 R0 几何句柄到 upstream FCL `CollisionObject` 的转换逻辑，供 `upstream_bridge.cpp`、宽相管理等模块共用。
- `src/kernel/selftest/src/self_test.cpp`：集中维护 `IOCTL_FCL_SELF_TEST` 所需的各类场景与回归逻辑，其他模块无需直接依赖。
- 其余子目录（`collision/`、`distance/`、`broadphase/` 等）各自聚焦对应 API，仅通过 `fclmusa/*.h` 引出公共接口。

## 构建指引

1. 安装 **Visual Studio 2022 + WDK 10.0.22621**（必须包含 *WindowsKernelModeDriver10.0* 工具集）。
2. 直接打开 `src/kernel/FclMusaDriver/FclMusaDriver.sln`（已预置 Debug/Release x64 配置）。
3. 运行 `external/Musa.Runtime/BuildAllTargets.cmd`，在 `external/Musa.Runtime/Publish/Library/<Config>/<Platform>` 下生成 `ucxxrt.lib`。
4. `FclMusaDriver.vcxproj` 自动导入 `../external/Musa.Runtime/Publish/Config/Musa.Runtime.Config.props`，如需调整路径请同步修改 `ImportGroup`。
5. 项目已启用 C++17、/kernel、/driver 以及池标签、日志配置；如需自定义请在 VS 属性页中覆盖。

> 如未安装 WDK 或未勾选 **WindowsKernelModeDriver10.0** 工具集，`msbuild` 会报 `MSB8020`。请先从微软官网下载并安装 WDK 10.0.22621，再运行 `BuildAllTargets.cmd` 与解决方案生成。

## 验证步骤

1. 部署驱动后，在 WinDbg（或 Test App）调用 `DeviceIoControl(IOCTL_FCL_PING)`，确认返回结构体：
   - `Version` 与 `FCL_MUSA_DRIVER_VERSION_*` 一致
   - `IsInitialized == TRUE`、`LastError == STATUS_SUCCESS`
   - `Pool.BytesInUse == 0`（空闲状态）
2. 在 `DriverEntry` 日志中可看到：
   - `Driver initialized successfully`
   - `Musa.Runtime STL smoke test passed`
   - 若缺少 Eigen，将看到 `Eigen headers not found; skipping math self-test`
3. 重复加载驱动验证：
   - 第二次调用 `FclInitialize()` 返回 `STATUS_ALREADY_INITIALIZED`
   - PING 中 `IsInitializing == FALSE`
4. 触发 `FclCleanup()`（卸载驱动）后检查池标签 `FCL ` 是否归零：`!poolused 2 FCL`
5. 在 PASSIVE_LEVEL 调用 `DeviceIoControl(IOCTL_FCL_SELF_TEST)`，校验：
   - `Passed == TRUE` 且 `OverallStatus == STATUS_SUCCESS`
   - `poolBefore.BytesInUse == poolAfter.BytesInUse`，即 `PoolBytesDelta == 0`
   - `CollisionDetected == TRUE`，并查看 `Contact.PenetrationDepth` 是否与默认测试配置（半径差）一致
   - Mesh-Mesh 与 Sphere-Mesh 两条 GJK 路径会在自检日志中分别出现“分离/穿透”阶段结果  
   （可直接运行 `pwsh -File tools/fcl-self-test.ps1` 自动完成 Ping + SelfTest 并返回 JSON/表格结果）

## 后续扩展

- 将 `FclRunEigenSmokeTest()` 改为在自检 IOCTL 中触发，并将结果存入诊断日志
- 在 `FclRunMusaRuntimeSmokeTests()` 中添加更多容器/算法（如 `std::optional`）以覆盖 FCL 需要的 STL 特性
- 引入真实 Eigen 头文件后，可在 `math/eigen_config.h` 里进一步裁剪（SIMD、对齐）

## 工具

- `tools/fcl-self-test.ps1`：PowerShell 自检脚本，调用 `IOCTL_FCL_PING` 与 `IOCTL_FCL_SELF_TEST`，支持 `-Json` 输出，CI/WinDbg 均可复用（退出码：0=成功，10=自检失败，2=系统错误）。
