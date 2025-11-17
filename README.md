# FCL+Musa Driver

FCL+Musa 是一个面向 Windows 内核（Ring 0）的碰撞检测驱动。项目将 Flexible Collision Library（FCL）与 Musa.Runtime、Eigen、libccd 进行裁剪整合，使 Sphere / OBB / Mesh 及连续碰撞（CCD）在内核态可直接调用，同时保留 BVH/OBBRSS 加速结构与自检能力。

## 快速导航

- 🚀 **新用户**？先看 [快速开始](QUICKSTART.md)（3分钟上手）
- 📚 **所有文档**？浏览 [文档索引](docs/INDEX.md)
- 🔧 **构建问题**？参考 [使用指南](docs/usage.md)
- 📖 **API 查询**？查看 [API 文档](docs/api.md)
- 🏗️ **理解设计**？阅读 [架构说明](docs/architecture.md)

## 构建

```powershell
PS> git clone https://github.com/lusipad/FCLMua.git
PS> cd FCLMua
PS> tools\build_all.ps1 -Configuration Release   # 推荐：一键构建所有组件
# 或者
PS> tools\build_and_sign_driver.ps1             # 仅构建+签名驱动
# 或者
PS> tools\manual_build.cmd                      # 仅构建驱动（不签名）
```

说明：

- `tools/build_all.ps1` 会依次构建驱动、CLI Demo、GUI Demo，自动签名并打包到 `dist/bundle/` 目录（推荐）。
- `tools/build_and_sign_driver.ps1` 构建驱动并自动生成测试证书签名，产物在 `dist/driver/x64/{Debug|Release}/`。
- `tools/manual_build.cmd` 仅构建驱动不签名，适合 CI/自动化流水线。
- 所有脚本使用相同的解决方案（`kernel/FclMusaDriver/FclMusaDriver.sln`）。

> 依赖：WDK 10.0.26100.0、Visual Studio 2022、Musa.Runtime（仓库自带）、Eigen、libccd。

构建成功后目录结构：

- `dist/driver/x64/{Debug|Release}/`：驱动构建产物
  - `FclMusaDriver.sys / FclMusaDriver.pdb`：驱动及符号文件
  - `FclMusaTestCert.pfx / .cer`：测试证书
- `dist/bundle/x64/{Debug|Release}/`：完整发布包（驱动 + 演示程序）

## 安装与加载

1. 将 `FclMusaDriver.sys` 复制到目标机器，例如 `C:\Drivers\FclMusaDriver.sys`。
2. 若使用测试证书，执行（管理员 PowerShell）：
   ```cmd
   certutil -addstore Root dist\driver\x64\Release\FclMusaTestCert.cer
   certutil -addstore TrustedPublisher dist\driver\x64\Release\FclMusaTestCert.cer
   ```
3. 创建并启动驱动服务：
   ```cmd
   sc create FclMusa type= kernel binPath= C:\Drivers\FclMusaDriver.sys
   sc start FclMusa
   ```
4. 或使用管理脚本（管理员 PowerShell）：
   ```powershell
   # 安装并启动
   PS> tools\manage_driver.ps1 -Action Install
   PS> tools\manage_driver.ps1 -Action Start

   # 重启驱动
   PS> tools\manage_driver.ps1 -Action Restart

   # 卸载
   PS> tools\manage_driver.ps1 -Action Uninstall
   ```

## IOCTL 接口概览

### 诊断/查询接口（0x800-0x80F）
| IOCTL | 代码 | 说明 |
|-------|------|------|
| `IOCTL_FCL_PING` | 0x800 | 查询驱动版本、初始化状态、池使用情况 |
| `IOCTL_FCL_SELF_TEST` | 0x801 | 触发完整自检（几何/碰撞/CCD/压力/Verifier 等） |
| `IOCTL_FCL_SELF_TEST_SCENARIO` | 0x802 | 单场景自检（runtime\|sphere\|broadphase\|mesh\|ccd） |
| `IOCTL_FCL_QUERY_DIAGNOSTICS` | 0x803 | 查询性能计时统计（碰撞/距离/CCD） |

### 正式接口（0x810-0x83F）
| IOCTL | 代码 | 说明 |
|-------|------|------|
| `IOCTL_FCL_QUERY_COLLISION` | 0x810 | 使用现有几何句柄执行碰撞检测 |
| `IOCTL_FCL_QUERY_DISTANCE` | 0x811 | 计算对象间最小距离与最近点 |
| `IOCTL_FCL_CREATE_SPHERE` | 0x812 | 创建球体几何，返回句柄 |
| `IOCTL_FCL_DESTROY_GEOMETRY` | 0x813 | 释放几何句柄 |
| `IOCTL_FCL_CREATE_MESH` | 0x814 | 传入顶点+索引缓冲，创建 Mesh 几何 |
| `IOCTL_FCL_CONVEX_CCD` | 0x815 | 运行 InterpMotion CCD，返回 TOI 信息 |

### 周期性碰撞接口（0x820-0x82F）
| IOCTL | 代码 | 说明 |
|-------|------|------|
| `IOCTL_FCL_START_PERIODIC_COLLISION` | 0x820 | 启动周期碰撞检测（DPC+PASSIVE 两级模型） |
| `IOCTL_FCL_STOP_PERIODIC_COLLISION` | 0x821 | 停止周期调度 |

### Demo 接口（0x900-0x9FF）
| IOCTL | 代码 | 说明 |
|-------|------|------|
| `IOCTL_FCL_DEMO_SPHERE_COLLISION` | 0x900 | Demo：创建两个球并返回碰撞测试结果（示例用途） |

详细结构定义见 `kernel/core/include/fclmusa/ioctl.h`。


## 用户态示例

```powershell
PS> tools\build_demo.cmd
PS> tools\build\fcl_demo.exe
```

CLI 提供命令：

**几何管理：**
- `sphere <name> <radius> [x y z]` - 创建球体对象
- `load <name> <obj>` - 加载 OBJ 文件并创建 Mesh
- `move <name> <x> <y> <z>` - 更新对象位姿
- `destroy <name>` - 销毁对象并释放句柄
- `list` - 列出所有场景对象

**碰撞查询：**
- `collide <A> <B>` - 静态碰撞检测
- `distance <A> <B>` - 距离查询
- `ccd <mov> <static> <dx> <dy> <dz>` - 连续碰撞检测（CCD）

**周期碰撞（DPC+PASSIVE 模型）：**
- `periodic <A> <B> <period_us>` - 启动周期碰撞检测（微秒）
- `periodic_stop` - 停止周期调度

**自检与诊断：**
- `selftest` - 完整自检（所有模块）
- `selftest <scenario>` - 场景自检（runtime|sphere|broadphase|mesh|ccd）
- `selftest_pass` - PASSIVE_LEVEL 多次检测自检（640次）
- `selftest_dpc` - DPC 周期自检（640ms@1ms周期）
- `diag` - 查询性能计时统计
- `diag_pass` / `diag_dpc` - 查看自检前后性能对比

**其他：**
- `run <script>` - 执行场景脚本（如 `run scenes\two_spheres.txt`）
- `help` - 显示命令帮助

场景脚本示例位于 `tools/scenes/` 目录。

## 内核态调用

- 在 `DriverEntry` 中调用 `FclInitialize()`，`DriverUnload` 中调用 `FclCleanup()`。
- API 位于 `kernel/core/include/fclmusa/*.h`，例如：
  - **几何管理**：`FclCreateGeometry / FclDestroyGeometry / FclAcquireGeometryReference`
  - **碰撞检测**：`FclCollideObjects / FclCollisionDetect`
  - **距离计算**：`FclDistanceCompute`
  - **连续碰撞**：`FclInterpMotionInitialize / FclContinuousCollision`
  - **周期碰撞**：`FclStartPeriodicCollision / FclStopPeriodicCollision`
- **IRQL 要求**：
  - 大多数 API 要求在 `PASSIVE_LEVEL` 调用
  - 周期碰撞回调在 `PASSIVE_LEVEL` 执行（由 DPC 触发，工作线程执行）
  - 快照版本的 Core API（使用 `FCL_GEOMETRY_SNAPSHOT`）可在 `DISPATCH_LEVEL` 调用
- 几何句柄生命周期由驱动管理，使用引用计数机制。

## 文档与工具

### 文档
- `docs/usage.md`：快速使用指南
- `docs/deployment.md`：证书、服务安装与卸载说明
- `docs/testing.md`：自检/压力/对比验证步骤
- `docs/VM_DEBUG_SETUP.md`：Hyper-V + WinDbg 配置手册
- `docs/FILE_STRUCTURE.md`：完整目录结构说明

### 构建工具
- `tools/build_all.ps1`：一键构建驱动、CLI Demo、GUI Demo 并打包
- `tools/build_and_sign_driver.ps1`：构建并签名驱动
- `tools/package_bundle.ps1`：将构建产物打包到 dist/bundle/

### 管理工具
- `tools/manage_driver.ps1`：驱动服务管理（安装/启动/停止/卸载/重启）
- `tools/fcl-self-test.ps1`：调用 `PING + SELF_TEST`
- `tools/verify_upstream.ps1`：对比驱动输出与 upstream FCL 的参考结果

### CI/CD
- `.github/workflows/build.yml`：GitHub Actions 自动构建用户态 CLI Demo

## Upstream 版本

- `fcl-source/` 内置的 FCL 基于 commit `5f7776e2101b8ec95d5054d732684d00dac45e3d`。
- `tools/manual_build.cmd`/`build_driver.cmd` 会检查 `fcl-source` HEAD 是否匹配该提交，避免混用其它版本。
- 若需升级 upstream FCL，请在 `fcl-source/` 同步代码并更新脚本中的 `FCL_EXPECTED_COMMIT`。





