# 构建指南

## 快速构建

有两种方式构建 FCL+Musa 项目：

### 方式 1：交互式菜单（推荐新手）

在项目根目录运行：

```powershell
.\build.ps1
```

然后选择你需要的构建选项：
- `[1]` 构建驱动 (Debug)
- `[2]` 构建驱动 (Release)
- `[3]` build_all (Debug)
- `[4]` build_all (Release + BuildRelease)
- `[5]` 仅构建 R3 Demo
- `[6]` 运行 R3 Smoke Test (cmake + ctest)
- `[0]` 退出

### 方式 2：命令行构建（推荐熟练用户）

#### 构建所有组件
```powershell
# Debug 版本
.\tools\build_all.ps1 -Configuration Debug

# Release 版本
.\tools\build_all.ps1 -Configuration Release

# Release 版本 + 签名
.\tools\build_all.ps1 -Configuration Release -Sign

# Release 版本 + 签名 + 打包
.\tools\build_all.ps1 -Configuration Release -Sign -Package
```

#### 仅构建驱动
```powershell
# Debug 版本
.\tools\manual_build.cmd Debug

# Release 版本
.\tools\manual_build.cmd Release
```

#### 仅构建用户态组件
```powershell
# CLI Demo
.\tools\build_demo.cmd

# GUI Demo
.\tools\gui_demo\build_gui_demo.cmd
```

## 常见问题

### Q: 构建失败，提示找不到 Visual Studio 或 WDK
**A:** 确保已安装：
- Visual Studio 2022
- WDK 10.0.22621.0
- 使用管理员权限运行 PowerShell

### Q: 构建成功但有很多警告
**A:** 这是正常的。警告主要来自：
- C4819: 文件编码警告（不影响功能）
- C4127: 条件表达式是常量（Eigen 模板库产生）
- 其他 Eigen 和 FCL 库的警告

这些警告不影响驱动的功能和稳定性。

### Q: 想要修改构建配置
**A:** 编辑 `.github/workflows/build.yml` 或项目设置文件

### Q: 构建输出在哪里
**A:** 
- 驱动：`kernel/FclMusaDriver/out/x64/{Debug|Release}/`
- CLI Demo：`tools/build/`
- GUI Demo：`tools/gui_demo/build/`
- 打包输出：`dist/bundle/x64/{Debug|Release}/`

## 下一步

构建成功后：
1. 查看 [QUICKSTART.md](QUICKSTART.md) 了解如何安装和运行
2. 查看 [docs/usage.md](docs/usage.md) 了解详细使用说明
3. 查看 [docs/api.md](docs/api.md) 了解 API 文档
