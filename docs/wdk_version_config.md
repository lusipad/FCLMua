# WDK 版本配置指南

FCL+Musa 构建系统支持灵活的 WDK 版本配置，您可以通过多种方式指定要使用的 WDK 版本。

## 配置优先级

配置优先级从高到低：

1. **命令行参数** （最高优先级）
2. **环境变量**
3. **配置文件**
4. **自动检测** （最低优先级）

## 方式 1：命令行参数（推荐）

直接在构建命令中指定 WDK 版本：

```powershell
# 使用主构建脚本
.\tools\build_all.ps1 -WdkVersion 10.0.26100.0

# 使用简化构建脚本
.\tools\build_simplified.ps1 -WdkVersion 10.0.26100.0

# 结合其他参数
.\tools\build_all.ps1 -Configuration Release -WdkVersion 10.0.22621.0 -Sign
```

**优点：**
- 明确指定，不会产生歧义
- 适合临时切换 WDK 版本
- 适合 CI/CD 环境

## 方式 2：环境变量

设置环境变量 `FCL_MUSA_WDK_VERSION`：

```powershell
# 临时设置（当前 PowerShell 会话）
$env:FCL_MUSA_WDK_VERSION = "10.0.26100.0"
.\tools\build_all.ps1

# Windows 永久设置（用户级）
[System.Environment]::SetEnvironmentVariable('FCL_MUSA_WDK_VERSION', '10.0.26100.0', 'User')

# Windows 永久设置（系统级，需要管理员权限）
[System.Environment]::SetEnvironmentVariable('FCL_MUSA_WDK_VERSION', '10.0.26100.0', 'Machine')
```

**优点：**
- 对所有构建脚本生效
- 适合个人开发环境配置
- 不需要修改代码或配置文件

**查看和删除环境变量：**

```powershell
# 查看当前值
$env:FCL_MUSA_WDK_VERSION

# 删除临时设置
Remove-Item Env:\FCL_MUSA_WDK_VERSION

# 删除永久设置
[System.Environment]::SetEnvironmentVariable('FCL_MUSA_WDK_VERSION', $null, 'User')
```

## 方式 3：配置文件

创建 `tools/build.config` 文件（基于 `build.config.example`）：

```powershell
# 1. 复制示例配置文件
Copy-Item tools\build.config.example tools\build.config

# 2. 编辑 tools\build.config
notepad tools\build.config
```

在配置文件中设置：

```ini
# FCL+Musa 构建配置文件
WdkVersion=10.0.26100.0
```

**优点：**
- 团队共享配置（可以加入版本控制）
- 集中管理构建参数
- 适合项目级配置

**注意：** `build.config` 文件已被添加到 `.gitignore`，如果需要团队共享，可以将配置加入版本控制。

## 方式 4：自动检测

如果没有指定 WDK 版本，构建系统会自动检测系统上已安装的 WDK：

```powershell
# 不指定任何 WDK 版本，自动检测
.\tools\build_all.ps1
```

**检测顺序：**
1. `10.0.22621.0` （项目推荐版本）
2. `10.0.26100.0`
3. `10.0.22000.0`

**优点：**
- 零配置，开箱即用
- 适合快速开始

**缺点：**
- 不同机器可能使用不同版本
- 可能导致构建结果不一致

## 实际应用场景

### 场景 1：开发环境（本地机器）

使用环境变量：

```powershell
$env:FCL_MUSA_WDK_VERSION = "10.0.26100.0"
# 之后所有构建都使用这个版本
```

### 场景 2：CI/CD 环境

使用命令行参数：

```powershell
# GitHub Actions / Azure DevOps
.\tools\build_all.ps1 -Configuration Release -WdkVersion $env:WDK_VERSION -Sign
```

### 场景 3：团队项目

使用配置文件：

```powershell
# tools/build.config
WdkVersion=10.0.22621.0
Configuration=Release
```

### 场景 4：临时测试不同版本

使用命令行参数覆盖：

```powershell
# 临时测试新版本 WDK
.\tools\build_all.ps1 -WdkVersion 10.0.26100.0
```

## 检查当前配置

构建时会显示使用的 WDK 版本：

```
Building FCL+Musa Driver...
Configuration: Debug | x64

[1/4] Setting up dependencies
[2/4] Building kernel driver (Debug|x64)
  Auto-detected WDK: 10.0.26100.0
  ✓ Driver built
```

## 查看系统上安装的 WDK 版本

```powershell
# 列出所有已安装的 WDK 版本
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" -Directory |
    Where-Object { Test-Path "$($_.FullName)\km\ntddk.h" } |
    Select-Object Name
```

## 常见问题

### Q: 如何知道我应该使用哪个 WDK 版本？

**A:**
- 项目推荐使用 `10.0.22621.0`（Windows 11 SDK）
- 如果系统上没有，使用 `10.0.26100.0` 也完全兼容
- 查看项目文档或团队规范

### Q: 命令行参数会覆盖环境变量吗？

**A:** 是的。优先级顺序：命令行 > 环境变量 > 配置文件 > 自动检测

### Q: 配置文件应该加入版本控制吗？

**A:**
- `build.config.example` - 应该加入（模板）
- `build.config` - 取决于团队需求
  - 如果团队需要统一配置 → 加入版本控制
  - 如果每个开发者有不同需求 → 不加入（已在 `.gitignore` 中）

### Q: 如何清除所有配置，使用自动检测？

**A:**
```powershell
# 1. 不传递 -WdkVersion 参数
# 2. 删除环境变量
Remove-Item Env:\FCL_MUSA_WDK_VERSION
# 3. 删除或注释配置文件中的 WdkVersion
```

## 项目文件更新说明

从原来硬编码 WDK 路径到动态版本的迁移：

**旧方式（硬编码）：**
```xml
<AdditionalIncludeDirectories>
  ...\Include\10.0.22621.0\km;...
</AdditionalIncludeDirectories>
```

**新方式（动态）：**
```xml
<AdditionalIncludeDirectories>
  ...\Include\$(WindowsTargetPlatformVersion)\km;...
</AdditionalIncludeDirectories>
```

这样 MSBuild 会使用通过 `/p:WindowsTargetPlatformVersion=<版本>` 传递的版本，无需修改项目文件。
