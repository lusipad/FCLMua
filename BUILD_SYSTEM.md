# FCL+Musa 构建系统

## 快速开始

使用主入口脚本启动交互式菜单:

```powershell
pwsh build.ps1
```

## 上游补丁管理

- `external/fcl-source` 作为上游 FCL 子模块，固定在 `df2702ca5e703dec98ebd725782ce13862e87fc8`（官方 v0.7.0 发布版）。
- 内核模式适配集中在 `patches/fcl-kernel-mode.patch`，通过 `pwsh tools/scripts/apply_fcl_patch.ps1` 自动套用，`build.ps1` 启动时也会执行该脚本。
- 如果需要重新同步子模块，可运行 `git submodule update --init --recursive` 后再次执行补丁脚本；若想清理补丁，使用 `git -C external/fcl-source checkout -- .` 恢复干净目录。

## 菜单结构

### 1. Build - 编译项目
- **R0 Debug** - 编译 R0 驱动 (Debug + 签名)
- **R0 Release** - 编译 R0 驱动 (Release + 签名)
- **R3 Lib Debug** - 仅编译 R3 用户态库 (Debug)
- **R3 Lib Release** - 仅编译 R3 用户态库 (Release)
- **R3 Demo Debug** - 编译 R3 Demo (Debug)
- **R3 Demo Release** - 编译 R3 Demo (Release)
- **CLI Demo** - 编译 CLI Demo
- **GUI Demo** - 编译 GUI Demo
- **All** - 编译所有项目

### 2. Test - 运行测试
- **R0 Demo** - 运行 R0 驱动测试
- **R3 Demo** - 运行 R3 Demo
- **GUI Demo** - 运行 GUI Demo

### 3. Doc - 生成文档
显示可用文档列表

### 4. Check Env - 检查环境
检查所有构建依赖:
- Visual Studio
- MSBuild
- Windows Driver Kit (WDK)
- CMake
- Musa.Runtime

### 5. Check Upstream - 检查上游更新
检查以下组件的更新:
- FCL 库
- Musa.Runtime NuGet包

## 文件结构

```
build.ps1                      # 主入口脚本（交互式菜单）
tools/
  build/                       # 构建系统
    common.psm1                # 公共函数库
    build-tasks.ps1            # 编译任务（R0/R3/CLI/GUI/All）
    test-tasks.ps1             # 测试任务（R0/R3/GUI）
    doc-tasks.ps1              # 文档任务
    check-env.ps1              # 环境检查
    check-upstream.ps1         # 上游更新检查
    assets/                    # 资源文件（模型等）
    scenes/                    # 场景配置文件
  scripts/                     # 工具脚本
    setup_dependencies.ps1     # 依赖设置脚本（Musa.Runtime）
    sign_driver.ps1            # 驱动签名脚本
    manage_driver.ps1          # 驱动管理脚本
    package_bundle.ps1         # 打包脚本
    run_all_tests.ps1          # 测试运行脚本
    verify_upstream_sources.ps1 # 上游源码验证
    nuget.exe                  # NuGet包管理工具
  build_demo.ps1               # CLI/GUI Demo 编译脚本
  build.config.example         # 构建配置示例
samples/                       # 示例项目
  cli_demo/                    # CLI示例
    main.cpp                   # CLI示例源码
    compile.cmd                # 编译脚本
  gui_demo/                    # GUI示例
    src/                       # 源码目录
    main.cpp                   # 主入口
    *.vcxproj                  # VS工程文件
  r3_user_demo/                # R3用户态示例
    main.cpp                   # R3示例源码
```

## 直接调用脚本

也可以直接调用各个脚本:

```powershell
# 检查环境
pwsh tools/build/check-env.ps1

# 编译 R0 Debug 驱动
pwsh tools/build/build-tasks.ps1 -Task R0-Debug

# 编译 R3 Release Demo
pwsh tools/build/build-tasks.ps1 -Task R3-Release

# 运行 R3 Demo
pwsh tools/build/test-tasks.ps1 -Task R3-Demo
```

## 依赖要求

- Windows 10/11
- PowerShell 7+ (pwsh)
- Visual Studio 2022 (with C++ workload)
- Windows Driver Kit (WDK) 10.0.26100.0 或更高版本
- CMake 3.24+

## 功能特点

1. **简洁设计** - 每个脚本专注于一个任务
2. **自动环境检测** - 自动查找 Visual Studio, WDK, CMake
3. **依赖管理** - 自动下载和缓存 Musa.Runtime
4. **错误处理** - 失败时显示错误信息，按回车返回菜单
5. **签名集成** - R0 驱动编译后自动签名

## 目录结构改进

最近的重构已完成以下改进：

1. **示例项目统一管理** - 所有示例项目移至 `samples/` 目录
   - `samples/cli_demo/` - CLI示例（原 `tools/cli_demo_build/`）
   - `samples/gui_demo/` - GUI示例（原 `tools/gui_demo/`）
   - `samples/r3_user_demo/` - R3用户态示例（原 `r3/samples/user_demo/`）

2. **工具脚本集中管理** - 辅助脚本移至 `tools/scripts/` 目录
   - 依赖管理、驱动签名、打包等工具统一管理
   - `tools/` 根目录仅保留高层构建脚本

3. **资源文件去重** - 资源文件统一保存在 `tools/build/` 目录
   - `tools/build/assets/` - 模型资源
   - `tools/build/scenes/` - 场景配置

4. **废弃脚本清理** - 已删除以下废弃脚本：
   - 测试脚本：`test_compile*.cmd/ps1`、`simple_compile.cmd`
   - 旧构建脚本：`build_demo.cmd`、`manual_build.ps1`
   - 重复的公共库：`tools/common.psm1`（使用 `tools/build/common.psm1`）

## 技术说明

- R0 Debug 编译时使用 Release 版本的 Musa.Runtime (链接兼容性)
- 驱动签名使用测试证书 (FclMusaTestCert)
- R3 Demo 通过根目录的 CMakeLists.txt 构建
- CLI/GUI Demo 使用 cl.exe 直接编译
