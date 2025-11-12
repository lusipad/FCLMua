# FCL+Musa Driver

在 Windows 内核 (Ring 0) 中移植 FCL（Flexible Collision Library），配合 Musa.Runtime/Eigen/libccd，实现 Sphere/OBB/Mesh 碰撞、连续碰撞 (CCD)、BVH/OBBRSS 支持以及内置自检。

## 构建

```powershell
PS> git clone … FCL+Musa
PS> cd FCL+Musa
PS> .\build_driver.cmd         # 生成 kernel/FclMusaDriver/out/x64/Debug/FclMusaDriver.sys
```

依赖：WDK 10.0.26100.0、Visual Studio 2022、Musa.Runtime（仓库已包含）。

## 安装 / 启动

```cmd
sc create FclMusa type= kernel binPath= C:\path\FclMusaDriver.sys
sc start FclMusa
```

> 需测试签名或自签证书。

## IOCTL 功能

| IOCTL | 说明 |
|-------|------|
| `IOCTL_FCL_PING` | 查询版本、初始化状态、内存统计 |
| `IOCTL_FCL_SELF_TEST` | 执行自检（几何/碰撞/CCD/压力/Verifier 等） |
| `IOCTL_FCL_CREATE_SPHERE` / `IOCTL_FCL_DESTROY_GEOMETRY` | 管理几何句柄 |
| `IOCTL_FCL_SPHERE_COLLISION` | Demo：驱动内部创建球体并返回碰撞结果 |
| `IOCTL_FCL_CONVEX_CCD` | Demo：使用 InterpMotion 进行连续碰撞 |
| `IOCTL_FCL_QUERY_COLLISION` / `IOCTL_FCL_QUERY_DISTANCE` | 通用碰撞/距离查询 |

更多结构/字段见 `docs/api.md`、`docs/demo.md`。

## 用户态示例

```powershell
PS> tools\build_demo.cmd
PS> tools\build\fcl_demo.exe
```

程序会依次演示球体碰撞、连续碰撞 IOCTL。

## 内核示例

在 Driver 代码中（PASSIVE_LEVEL）可参考 `docs/demo.md`，直接调用 `FclCreateGeometry`、`FclCollideObjects`、`FclContinuousCollision` 等 API。使用前确保 `FclInitialize()` 成功，卸载前调用 `FclCleanup()`。

## 文档

- `docs/api.md`：API 说明
- `docs/usage.md`：使用指南
- `docs/architecture.md`：架构概述
- `docs/testing.md`：测试报告
- `docs/known_issues.md`：已知问题
- `docs/release_notes.md`：发布说明
- `docs/deployment.md`：部署流程
- `docs/demo.md`：Ring0/Ring3 示例

## 现状

- Debug 驱动已实现 BVH/OBBRSS、GJK/EPA、InterpMotion/ScrewMotion、Conservative Advancement CCD。
- 自测覆盖几何更新、Sphere/OBB、复杂 Mesh、边界、连续碰撞、Driver Verifier、压力/性能等路径（可通过 `IOCTL_FCL_SELF_TEST` 获取结果）。
- TODO：搭建 WinDbg 调试链路、准备 Release 签名、完善 WinDbg/Verifier 操作手册等（详见 `docs/known_issues.md`）。
