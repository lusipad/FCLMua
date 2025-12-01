# CI 构建问题诊断总结

## ✅ 已修复的问题

### 1. User-mode Build Parallelization
**问题**: 并行构建导致 GUI Demo 找不到 R3 Lib  
**修复**: 将 R3 Lib + GUI Demo 放入同一 job 内串行执行  
**状态**: ✅ 已解决

### 2. WDK 安装 - windows-2022 缺少 winget
**问题**: `windows-2022` runner 没有预装 winget  
**修复**: 添加回退机制，检测 winget 可用性，不可用时下载官方 WDK 安装程序  
**状态**: ✅ 已解决

### 3. setup_dependencies.ps1 路径错误
**问题**: `Build-R0Driver` 中路径为 `tools\setup_dependencies.ps1`，实际在 `tools\scripts\`  
**修复**: 更正路径为 `tools\scripts\setup_dependencies.ps1`  
**状态**: ✅ 已解决

### 4. setup_dependencies.ps1 仓库根目录计算错误
**问题**: `$repoRoot = Join-Path $scriptDir '..'` → 结果是 `tools/` 而不是仓库根目录  
**影响**: Musa.Runtime 被安装到 `tools/external/Musa.Runtime/` 错误位置  
**修复**: 更正为 `Join-Path $scriptDir '../..'`  
**状态**: ✅ 已解决

### 5. Selftest 路径错误
**问题**: 项目文件引用 `kernel/selftest/` 但实际目录是 `kernel/tests/`  
**修复**: 更正项目文件路径  
**状态**: ✅ 已解决

## ❌ 待解决的问题

### WDK Driver Build - NuGet 依赖缺失

**症状**: 链接错误，30+ 未定义符号
```
- MusaCoreStartup / MusaCoreShutdown  
- __imp_TlsAlloc / __imp_GetLastError / __imp_SetLastError
- __imp_HeapAlloc / __imp_HeapFree
- 等等...
```

**根本原因**: 项目依赖三个 NuGet 包，但 CI 中只安装了一个

| 包名 | 版本 | 安装位置 | CI 状态 |
|------|------|----------|---------|
| Musa.Runtime | 0.5.1 | `external/Musa.Runtime/Publish/` | ✅ 已安装 |
| Musa.Core | 0.4.1 | `$(USERPROFILE)\.nuget\packages\` | ❌ 缺失 |
| Musa.CoreLite | 1.0.3 | `$(USERPROFILE)\.nuget\packages\` | ❌ 缺失 |

**影响**:
- `MusaCoreStartup` 和 `MusaCoreShutdown` 在 `Musa.Core.lib` 中
- 所有 Windows API (`TlsAlloc`, `HeapAlloc` 等) 的内核模式实现在 Musa.Core/CoreLite 中
- 缺少这些包导致链接器找不到符号定义

**vcxproj 中的导入**:
```xml
<Import Project="$(USERPROFILE)\.nuget\packages\musa.corelite\1.0.3\build\native\Config\Musa.CoreLite.Config.props" 
        Condition="exists(...)" />
<Import Project="$(USERPROFILE)\.nuget\packages\musa.core\0.4.1\build\native\Config\Musa.Core.Config.props" 
        Condition="exists(...)" />
```
由于 `Condition="exists(...)"` 检查失败，导入被跳过，链接器配置不完整。

**解决方案选项**:

1. **扩展 setup_dependencies.ps1** (推荐)
   - 支持安装多个 NuGet 包
   - 统一管理所有依赖项
   - 保持与本地开发一致

2. **使用 NuGet restore**
   - 创建 `packages.config`
   - 使用 `nuget restore` 恢复所有包
   - 标准化的 NuGet 工作流

3. **将所有依赖项放入 external/**
   - 类似 Musa.Runtime 的方式
   - 完全控制依赖版本
   - 不依赖用户 NuGet 缓存

**当前状态**: 
- WDK workflow 禁用了自动触发
- 保留手动触发选项用于测试
- 需要实现完整的 NuGet 依赖管理

## CI 工作流当前状态

| Workflow | 自动触发 | 状态 |
|----------|----------|------|
| User-mode Build | ✅ 启用 | ✅ 通过 |
| WDK Driver Build | ❌ 禁用 (仅手动) | ❌ NuGet 依赖缺失 |

## 后续行动

1. **短期**: 保持 WDK workflow 手动触发状态
2. **中期**: 实现完整的 NuGet 依赖管理
3. **长期**: 考虑将所有依赖项统一到 `external/` 目录，简化 CI 配置

## 相关文件

- `.github/workflows/build.yml` - User-mode 构建 (✅ 工作)
- `.github/workflows/wdk-driver.yml` - Driver 构建 (❌ 禁用)
- `tools/scripts/setup_dependencies.ps1` - Musa.Runtime 安装
- `tools/build/build-tasks.ps1` - 构建任务
- `tools/build/common.psm1` - 通用构建函数
- `kernel/driver/msbuild/FclMusaDriver.vcxproj` - 驱动项目文件
