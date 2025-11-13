# FCL+Musa Driver

FCL+Musa 是一个面向 Windows 内核（Ring 0）的碰撞检测驱动。项目将 Flexible Collision Library（FCL）与 Musa.Runtime、Eigen、libccd 进行裁剪整合，使 Sphere / OBB / Mesh 及连续碰撞（CCD）在内核态可直接调用，同时保留 BVH/OBBRSS 加速结构与自检能力。

## 构建

```powershell
PS> git clone https://github.com/lusipad/FCLMua.git
PS> cd FCLMua
PS> tools\manual_build.cmd      # 无交互构建，适合 CI/自动化
# 或者
PS> .\build_driver.cmd          # 带交互输出，可自动调用签名脚本
```

说明：

- `tools/manual_build.cmd` 会初始化 VsDevCmd 与 WDK 环境，执行 `msbuild Clean+Build` 并停止在构建步骤，便于在自动化流水线中使用。
- `build_driver.cmd` 在构建成功后会继续执行 `tools/sign_driver.ps1`，生成/更新 `FclMusaTestCert.pfx/.cer` 并为 `FclMusaDriver.sys` 进行测试签名。
- 两个脚本使用相同的解决方案（`kernel/FclMusaDriver/FclMusaDriver.sln`），产物位于 `kernel/FclMusaDriver/out/x64/Debug/`。

> 依赖：WDK 10.0.26100.0、Visual Studio 2022、Musa.Runtime（仓库自带）、Eigen、libccd。

构建成功后目录包含：

- `FclMusaDriver.sys / FclMusaDriver.pdb`：驱动及符号文件
- `FclMusaTestCert.pfx / .cer`：测试证书（仅在 `build_driver.cmd` 路径下生成）

## 安装与加载

1. 将 `FclMusaDriver.sys` 复制到目标机器，例如 `C:\Drivers\FclMusaDriver.sys`。
2. 若使用测试证书，执行（管理员 PowerShell）：
   ```cmd
   certutil -addstore Root kernel\FclMusaDriver\out\x64\Debug\FclMusaTestCert.cer
   certutil -addstore TrustedPublisher kernel\FclMusaDriver\out\x64\Debug\FclMusaTestCert.cer
   ```
3. 创建并启动驱动服务：
   ```cmd
   sc create FclMusa type= kernel binPath= C:\Drivers\FclMusaDriver.sys
   sc start FclMusa
   ```
4. 卸载/删除：`sc stop FclMusa`，`sc delete FclMusa`。

## IOCTL 接口概览

| IOCTL                       | 说明                                            |
|-----------------------------|-------------------------------------------------|
| `IOCTL_FCL_PING`            | 查询驱动版本、初始化状态、池使用情况           |
| `IOCTL_FCL_SELF_TEST`       | 触发自检（几何/碰撞/CCD/压力/Verifier 等）      |
| `IOCTL_FCL_CREATE_SPHERE`   | 创建球体几何，返回句柄                          |
| `IOCTL_FCL_DESTROY_GEOMETRY`| 释放几何句柄                                    |
| `IOCTL_FCL_SPHERE_COLLISION`| Demo：创建两个球并返回碰撞测试结果              |
| `IOCTL_FCL_QUERY_COLLISION` | 使用现有句柄执行碰撞检测                        |
| `IOCTL_FCL_QUERY_DISTANCE`  | 计算对象间最小距离与最近点                     |
| `IOCTL_FCL_CONVEX_CCD`      | 运行 InterpMotion CCD，返回 TOI 信息            |
| `IOCTL_FCL_CREATE_MESH`     | 传入顶点+索引缓冲，创建 Mesh 几何               |

详细结构定义见 `kernel/core/include/fclmusa/ioctl.h`。

## 用户态示例

```powershell
PS> tools\build_demo.cmd
PS> tools\build\fcl_demo.exe
```

CLI 提供命令：

- `sphere <name> <radius> [x y z]`：创建球体对象
- `move <name> <x> <y> <z>`：更新位姿
- `collide <A> <B>` / `distance <A> <B>`：静态碰撞/距离查询
- `simulate <mov> <static> <dx> <dy> <dz> <steps> <interval_ms>`：增量移动后逐步碰撞检测
- `ccd <mov> <static> <dx> <dy> <dz>`：执行连续碰撞
- `load <name> <obj>`：加载 OBJ 并创建 Mesh
- `destroy <name>` / `list` / `help` 等

也可直接执行 `run scenes\two_spheres.txt` 等脚本在 `tools/scenes` 下重放预设场景。

## 内核态调用

- 在 `DriverEntry` 中调用 `FclInitialize()`，`DriverUnload` 中调用 `FclCleanup()`。
- API 位于 `kernel/core/include/fclmusa/*.h`，例如：
  - `FclCreateGeometry / FclDestroyGeometry`
  - `FclCollideObjects / FclCollisionDetect`
  - `FclDistanceCompute`
  - `FclInterpMotionInitialize / FclContinuousCollision`
- 所有 API 要求在 `PASSIVE_LEVEL` 调用，几何句柄生命周期由驱动管理。

## 文档与工具

- `docs/usage.md`：快速使用指南
- `docs/deployment.md`：证书、服务安装与卸载说明
- `docs/testing.md`：自检/压力/对比验证步骤
- `docs/VM_DEBUG_SETUP.md`：Hyper-V + WinDbg 配置手册
- `docs/FILE_STRUCTURE.md`：完整目录结构说明
- `tools/fcl-self-test.ps1`：调用 `PING + SELF_TEST`
- `tools/verify_upstream.ps1`：对比驱动输出与 upstream FCL 的参考结果

## Upstream 版本

- `fcl-source/` 内置的 FCL 基于 commit `5f7776e2101b8ec95d5054d732684d00dac45e3d`。
- `tools/manual_build.cmd`/`build_driver.cmd` 会检查 `fcl-source` HEAD 是否匹配该提交，避免混用其它版本。
- 若需升级 upstream FCL，请在 `fcl-source/` 同步代码并更新脚本中的 `FCL_EXPECTED_COMMIT`。




