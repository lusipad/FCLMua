# WDK 版本配置 - 快速参考

## 三种配置方式

### 1️⃣ 命令行参数（推荐 ⭐）

```powershell
# 基本用法
.\tools\build_all.ps1 -WdkVersion 10.0.26100.0

# 完整示例
.\tools\build_all.ps1 -Configuration Release -WdkVersion 10.0.26100.0 -Sign

# 简化构建脚本
.\tools\build_simplified.ps1 -WdkVersion 10.0.26100.0
```

### 2️⃣ 环境变量

```powershell
# 设置环境变量
$env:FCL_MUSA_WDK_VERSION = "10.0.26100.0"

# 然后正常构建
.\tools\build_all.ps1
```

### 3️⃣ 配置文件

```powershell
# 创建配置文件
Copy-Item tools\build.config.example tools\build.config

# 编辑文件，添加：
# WdkVersion=10.0.26100.0

# 然后正常构建
.\tools\build_all.ps1
```

## 配置优先级

```
命令行参数 > 环境变量 > 配置文件 > 自动检测
```

## 查看可用的 WDK 版本

```powershell
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" -Directory |
    Where-Object { Test-Path "$($_.FullName)\km\ntddk.h" } |
    Select-Object Name
```

## 测试配置

```powershell
# 运行配置测试脚本
.\tools\test_wdk_config.ps1
```

## 常见场景

| 场景 | 推荐方式 | 示例 |
|------|---------|------|
| 日常开发 | 自动检测 | `.\tools\build_all.ps1` |
| 临时测试不同版本 | 命令行参数 | `.\tools\build_all.ps1 -WdkVersion 10.0.26100.0` |
| 个人开发环境 | 环境变量 | `$env:FCL_MUSA_WDK_VERSION = "10.0.26100.0"` |
| 团队项目 | 配置文件 | 在 `build.config` 中设置 |
| CI/CD | 命令行参数 | 脚本中显式指定 |

## 详细文档

查看完整文档：[docs/wdk_version_config.md](../docs/wdk_version_config.md)
