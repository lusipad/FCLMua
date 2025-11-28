# 快速开始（3分钟）

本文档帮助你在3分钟内完成 FCL+Musa 驱动的编译、安装和运行。

## 前置要求

- Windows 10/11 (x64)
- Visual Studio 2022
- WDK 10.0.22621.0
- 管理员权限

## 一键编译和运行

```powershell
# 1. 克隆仓库
git clone https://github.com/lusipad/FCLMua.git
cd FCLMua

# 2. 一键构建所有组件（驱动、CLI Demo、GUI Demo）
PS> tools\build_all.ps1 -Configuration Release

# 3. 安装并启动驱动
PS> tools\manage_driver.ps1 -Action Install
PS> tools\manage_driver.ps1 -Action Start

# 4. 运行 CLI Demo
PS> tools\build\fcl_demo.exe

# 5. 快速自检
PS> tools\fcl-self-test.ps1
```

## 常用命令速查

### 构建命令

```powershell
# 构建所有组件（推荐）
tools\build_all.ps1 -Configuration Release

# 构建所有组件并签名驱动
tools\build_all.ps1 -Configuration Release -Sign

# 完整发布流程（构建+签名+打包）
tools\build_all.ps1 -Configuration Release -Sign -Package

# 仅构建驱动并签名
tools\build_all.ps1 -DriverOnly -Sign -Configuration Release

# 仅构建驱动（不签名，适合 CI）
tools\manual_build.cmd

# 仅构建 Demo（跳过驱动）
tools\build_all.ps1 -SkipDriver

# 仅构建 CLI Demo
tools\build_demo.cmd

# 仅构建 GUI Demo
tools\gui_demo\build_gui_demo.cmd
```

### 驱动管理命令

```powershell
# 安装驱动服务
tools\manage_driver.ps1 -Action Install

# 启动驱动
tools\manage_driver.ps1 -Action Start

# 停止驱动
tools\manage_driver.ps1 -Action Stop

# 重启驱动
tools\manage_driver.ps1 -Action Restart

# 卸载驱动
tools\manage_driver.ps1 -Action Uninstall

# 重新安装（卸载+安装+启动）
tools\manage_driver.ps1 -Action Reinstall
```

### 测试命令

```powershell
# 驱动自检（完整测试）
tools\fcl-self-test.ps1

# 验证与上游 FCL 一致性
tools\verify_upstream.ps1

# 运行所有单元测试
tools\run_all_tests.ps1
```

### CLI Demo 交互命令

启动 CLI Demo 后，可以使用以下命令：

```text
# 几何管理
> sphere a 0.5                    # 创建球体 a（半径0.5）
> sphere b 0.5 1 0 0              # 创建球体 b（半径0.5，位置(1,0,0)）
> load cube assets/cube.obj       # 加载 OBJ 模型
> move a 0.1 0 0                  # 移动对象 a
> destroy a                       # 销毁对象 a
> list                            # 列出所有对象

# 碰撞查询
> collide a b                     # 静态碰撞检测
> distance a b                    # 距离查询
> ccd a b 2 0 0                   # 连续碰撞检测（CCD）

# 周期碰撞（DPC 模型）
> periodic a b 1000               # 启动周期碰撞（1ms = 1000us）
> periodic_stop                   # 停止周期调度
> diag                            # 查询性能统计

# 自检与诊断
> selftest                        # 完整自检
> selftest sphere                 # 球体场景自检
> selftest_dpc                    # DPC 周期自检

# 场景脚本
> run scenes\two_spheres.txt      # 执行预设场景

# 其他
> help                            # 显示帮助
> exit                            # 退出
```

## 典型使用场景

### 场景1：开发和调试

```powershell
# 编译驱动（Debug 模式）
tools\build_and_sign_driver.ps1 -Configuration Debug

# 安装并启动
tools\manage_driver.ps1 -Action Reinstall

# 运行自检验证
tools\fcl-self-test.ps1

# 使用 CLI Demo 交互测试
tools\build\fcl_demo.exe
```

### 场景2：发布版本

```powershell
# 完整发布流程（构建 Release + 签名 + 打包）
tools\build_all.ps1 -Configuration Release -Sign -Package

# 查看打包结果
ls dist\bundle\x64\Release\
# 输出：
# - FclMusaDriver.sys（已签名）
# - FclMusaDriver.pdb
# - FclMusaTestCert.cer
# - FclMusaTestCert.pfx
# - fcl_demo.exe
# - fcl_gui_demo.exe
```

### 场景3：CI/CD 集成

```powershell
# 仅构建（不签名）
tools\manual_build.cmd

# 或使用 GitHub Actions 自动化
# 见 .github\workflows\build.yml
```

## 获取帮助

遇到问题？查看相关文档：

| 问题类型 | 文档 |
|---------|------|
| 构建失败 | [使用指南](docs/usage.md) |
| 驱动加载失败 | [部署说明](docs/deployment.md) |
| API 使用 | [API 文档](docs/api.md) |
| 架构理解 | [架构说明](docs/architecture.md) |
| 测试验证 | [测试指南](docs/testing.md) |
| 调试技巧 | [VM 调试设置](docs/vm_debug_setup.md) |
| 已知问题 | [已知问题](docs/known_issues.md) |
| 所有文档 | [文档索引](docs/index.md) |

## 常见问题

**Q: 构建失败，找不到 WDK 头文件？**
A: 确认已安装 WDK 10.0.22621.0，并以管理员权限运行 PowerShell。

**Q: 驱动加载失败，错误 577？**
A: 需要导入测试证书或启用测试签名模式：
```cmd
certutil -addstore Root dist\driver\x64\Release\FclMusaTestCert.cer
certutil -addstore TrustedPublisher dist\driver\x64\Release\FclMusaTestCert.cer
```

**Q: IOCTL 超时或错误 0xC0000008？**
A: 确认驱动正在运行：`sc query FclMusa`，设备名使用 `\\.\FclMusa`。

**Q: 需要 Release 构建？**
A: 使用 `-Configuration Release` 参数：
```powershell
tools\build_all.ps1 -Configuration Release
```

**Q: 如何更新到最新代码？**
A:
```powershell
git pull origin main
tools\manage_driver.ps1 -Action Stop
tools\build_all.ps1 -Configuration Release
tools\manage_driver.ps1 -Action Start
```

## 下一步

- 🔍 深入学习 → [文档索引](docs/index.md)
- 📖 理解架构 → [架构说明](docs/architecture.md)
- 🛠️ API 开发 → [API 文档](docs/api.md)
- 🐛 故障排除 → [已知问题](docs/known_issues.md)
- 💡 贡献代码 → [贡献指南](CONTRIBUTING.md)（即将推出）

---

**提示**: 第一次使用建议完整阅读 [使用指南](docs/usage.md) 以了解完整的工作流程。
