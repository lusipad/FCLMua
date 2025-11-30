# PowerShell 迁移说明

## 概述

为了保持一致性和跨平台兼容性，我们正在将所有构建脚本迁移到 PowerShell (pwsh)。

## 已完成的迁移

### 核心构建脚本

| 旧文件 | 新文件 | 状态 | 说明 |
|--------|--------|------|------|
| `manual_build.cmd` | `manual_build.ps1` | ✅ 完成 | 驱动构建核心脚本 |
| `build.ps1` | `menu.ps1` | ✅ 完成 | 主菜单（旧文件已不存在） |

### 新的 PowerShell 脚本

所有新脚本都是纯 PowerShell：

```
build.ps1                      - 主菜单入口（交互式）
tools/
  ├── build/                  - 统一的任务脚本集合
  │   ├── common.psm1         - 公共函数
  │   ├── build-tasks.ps1     - 构建任务
  │   ├── test-tasks.ps1      - 测试任务
  │   ├── doc-tasks.ps1       - 文档任务
  │   ├── check-env.ps1       - 环境检查
  │   └── check-upstream.ps1  - 上游检查
  ├── build_demo.ps1          - CLI/GUI Demo 构建
  ├── setup_dependencies.ps1  - 依赖安装（Musa.Runtime）
  ├── sign_driver.ps1         - 驱动签名
  └── gui_demo/main.cpp       - GUI Demo 占位源码
```

说明：原 menu_*.ps1 与 legacy 辅助脚本已归档至 archive/，统一使用 tools/build/* 与 build.ps1。

## 保留的 CMD 文件

以下 CMD 文件暂时保留，因为它们是特定工具或 demo：

| 文件 | 用途 | 是否需要迁移 |
|------|------|--------------|
| `tools/build_demo.cmd` | Demo 构建 | 🔄 建议迁移 |
| `tools/test_vs_detect.cmd` | VS 检测测试 | ⚠️ 工具脚本 |
| `tools/gui_demo/*.cmd` | GUI Demo 相关 | 🔄 建议迁移 |

## PowerShell vs CMD 的优势

### 为什么使用 PowerShell？

1. **跨平台兼容**
   - PowerShell Core (pwsh) 可在 Windows, Linux, macOS 运行
   - CMD 仅限 Windows

2. **更好的错误处理**
   ```powershell
   # PowerShell
   $ErrorActionPreference = 'Stop'
   try { ... } catch { ... }
   
   # CMD - 错误处理困难
   if errorlevel 1 exit /b 1
   ```

3. **现代语法**
   ```powershell
   # PowerShell - 清晰的对象操作
   $vsPath = & vswhere.exe -latest -property installationPath
   
   # CMD - 复杂的字符串处理
   for /f "usebackq delims=" %%i in (`vswhere.exe ...`) do set "VS_PATH=%%i"
   ```

4. **更好的集成**
   - 与 .NET 集成
   - 丰富的 cmdlets
   - 包管理器支持

## 使用 pwsh 而非 powershell

### 区别

- **powershell**: Windows PowerShell 5.1（仅 Windows，旧版）
- **pwsh**: PowerShell 7+（跨平台，现代化）

### 推荐做法

```powershell
# 在脚本中调用其他 PowerShell 脚本时使用 pwsh
& pwsh -NoProfile -ExecutionPolicy Bypass -File script.ps1

# 而不是
& powershell -File script.ps1
```

### 好处

1. **更快的启动速度**
2. **更好的性能**
3. **跨平台一致性**
4. **现代特性支持**

## 迁移指南

### CMD 转 PowerShell 的常见模式

#### 1. 变量设置
```batch
# CMD
set "VAR=value"

# PowerShell
$VAR = 'value'
```

#### 2. 条件判断
```batch
# CMD
if "%VAR%"=="" (
    echo Variable not set
)

# PowerShell
if (-not $VAR) {
    Write-Host "Variable not set"
}
```

#### 3. 循环
```batch
# CMD
for /f "delims=" %%i in ('command') do set "VAR=%%i"

# PowerShell
$VAR = & command
```

#### 4. 路径操作
```batch
# CMD
set "SCRIPT_DIR=%~dp0"

# PowerShell
$ScriptDir = $PSScriptRoot
```

#### 5. 调用外部命令
```batch
# CMD
command arg1 arg2
if errorlevel 1 exit /b 1

# PowerShell
& command arg1 arg2
if ($LASTEXITCODE -ne 0) { exit 1 }
```

## manual_build.ps1 示例

新的 `manual_build.ps1` 展示了完整的迁移：

### 功能对比

| 功能 | CMD 版本 | PowerShell 版本 |
|------|----------|-----------------|
| VS 检测 | 复杂的 for 循环 | 简单的函数调用 |
| WDK 解析 | 临时批处理文件 | 直接调用 PowerShell |
| 环境变量 | call + set | [Environment]::SetEnvironmentVariable |
| 错误处理 | errorlevel | try/catch + $LASTEXITCODE |
| 输出 | echo | Write-Host（彩色） |

### 使用方法

```powershell
# 旧方式（CMD）
.\tools\manual_build.cmd Debug

# 新方式（PowerShell）
.\tools\manual_build.ps1 -Configuration Debug
pwsh -File .\tools\manual_build.ps1 -Configuration Debug

# 或从菜单调用
.\menu.ps1  # 选择 Build → R0 Debug
```

## 测试

所有新脚本都已经过测试：

```powershell
# 运行验证脚本
.\tools\verify_menu_system.ps1

# 测试构建脚本
.\tools\manual_build.ps1 -Configuration Debug -Verbose

# 完整菜单测试
.\menu.ps1
```

## 迁移进度

### ✅ 已完成
- [x] 主菜单系统（menu.ps1 + menu_*.ps1）
- [x] 核心构建脚本（manual_build.ps1）
- [x] 测试验证脚本
- [x] 文档和工具脚本

### 🔄 待迁移（可选）
- [ ] build_demo.cmd → build_demo.ps1
- [ ] gui_demo/*.cmd → gui_demo/*.ps1
- [ ] test_vs_detect.cmd → test_vs_detect.ps1

### ⚠️ 保留
- external/ 中的 CMD 文件（第三方依赖）

## 安装 PowerShell 7+

如果系统没有 pwsh，请安装：

### Windows
```powershell
# 使用 winget
winget install Microsoft.PowerShell

# 或下载安装包
# https://github.com/PowerShell/PowerShell/releases
```

### Linux
```bash
# Ubuntu/Debian
sudo apt-get install -y powershell

# CentOS/RHEL
sudo yum install -y powershell
```

### macOS
```bash
brew install powershell/tap/powershell
```

## 验证安装

```powershell
# 检查版本
pwsh --version

# 应输出类似：
# PowerShell 7.4.0
```

## 总结

- ✅ 核心构建系统已完全迁移到 PowerShell
- ✅ 使用 `pwsh` 而非 `powershell` 以获得最佳性能
- ✅ 新脚本支持跨平台（理论上，但驱动构建仅限 Windows）
- ✅ 更好的错误处理和可维护性
- ✅ 保持了所有功能的完整性

**推荐**: 优先使用新的 PowerShell 脚本，旧的 CMD 文件可以在验证后移除。
