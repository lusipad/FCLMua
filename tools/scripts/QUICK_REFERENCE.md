# FCL 补丁工具快速参考

完整的补丁管理工具套件，包含查看、对比、应用、生成和测试功能。

## 🎯 快速开始

### 最常用的命令

```powershell
# 1. 查看补丁摘要
pwsh tools/scripts/view_patch.ps1

# 2. 对比补丁优化效果
pwsh tools/scripts/diff_patch.ps1 -Mode patches

# 3. 查看特定文件修改
pwsh tools/scripts/view_patch.ps1 -File "include/fcl/logging.h"

# 4. 在 VS Code 中对比
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/logging.h" -Tool vscode
```

## 📋 工具总览

| 工具 | 功能 | 常用场景 |
|------|------|----------|
| `view_patch.ps1` | 查看补丁内容 | 了解补丁修改了什么 |
| `diff_patch.ps1` | 对比差异 | 审查具体修改内容 |
| `apply_fcl_patch.ps1` | 应用/恢复补丁 | 构建前应用补丁 |
| `regenerate_minimal_patch.ps1` | 重新生成补丁 | 更新补丁内容 |
| `test_minimal_patch.ps1` | 测试补丁 | 验证补丁有效性 |

## 🔍 查看补丁 - view_patch.ps1

### 5 种查看模式

```powershell
# 摘要模式（默认）- 快速了解
pwsh tools/scripts/view_patch.ps1

# 分类模式 - 按文件类型查看
pwsh tools/scripts/view_patch.ps1 -Mode category

# 统计模式 - 详细统计信息
pwsh tools/scripts/view_patch.ps1 -Mode stats

# 文件列表模式 - 简洁列表
pwsh tools/scripts/view_patch.ps1 -Mode files

# 查看特定文件
pwsh tools/scripts/view_patch.ps1 -File "include/fcl/logging.h"
```

### 输出示例

```
文件变更：
  总文件数:     64
  新增文件:     5
  修改文件:     59
  删除文件:     0

代码变更：
  新增行数:     +3879
  删除行数:     -3026
  净变更:       +853
```

## 🔄 对比差异 - diff_patch.ps1

### 4 种对比模式

```powershell
# 应用前后对比
pwsh tools/scripts/diff_patch.ps1

# 补丁文件对比（原始 vs 最小化）
pwsh tools/scripts/diff_patch.ps1 -Mode patches

# 单文件对比
pwsh tools/scripts/diff_patch.ps1 -Mode file -File "src/common/detail/profiler.cpp"

# 交互式并排对比
pwsh tools/scripts/diff_patch.ps1 -Mode side-by-side
```

### 工具选择

```powershell
# Git Diff（默认）
pwsh tools/scripts/diff_patch.ps1 -Tool git

# VS Code（推荐）
pwsh tools/scripts/diff_patch.ps1 -File "file.h" -Tool vscode
```

## 🔧 应用补丁 - apply_fcl_patch.ps1

```powershell
# 应用补丁
pwsh tools/scripts/apply_fcl_patch.ps1

# 恢复到干净状态
pwsh tools/scripts/apply_fcl_patch.ps1 -Restore

# 静默模式
pwsh tools/scripts/apply_fcl_patch.ps1 -Quiet
```

## 🔨 生成补丁 - regenerate_minimal_patch.ps1

```powershell
# 重新生成最小化补丁
pwsh tools/scripts/regenerate_minimal_patch.ps1

# Dry run 模式（仅预览）
pwsh tools/scripts/regenerate_minimal_patch.ps1 -DryRun
```

## ✅ 测试补丁 - test_minimal_patch.ps1

```powershell
# 测试补丁有效性
pwsh tools/scripts/test_minimal_patch.ps1
```

## 📖 完整工作流

### 场景 1：代码审查

```powershell
# 步骤 1：查看补丁概况
pwsh tools/scripts/view_patch.ps1 -Mode summary

# 步骤 2：对比补丁优化
pwsh tools/scripts/diff_patch.ps1 -Mode patches

# 步骤 3：按类型查看
pwsh tools/scripts/view_patch.ps1 -Mode category

# 步骤 4：审查关键文件
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/logging.h" -Tool vscode

# 步骤 5：查看详细差异
pwsh tools/scripts/diff_patch.ps1 -Mode side-by-side
```

### 场景 2：构建准备

```powershell
# 步骤 1：应用补丁
pwsh tools/scripts/apply_fcl_patch.ps1

# 步骤 2：验证应用
pwsh tools/scripts/diff_patch.ps1

# 步骤 3：构建
pwsh build.ps1
```

### 场景 3：补丁更新

```powershell
# 步骤 1：恢复干净状态
pwsh tools/scripts/apply_fcl_patch.ps1 -Restore

# 步骤 2：应用原始补丁
cd external/fcl-source
git apply ../../patches/fcl-kernel-mode.patch

# 步骤 3：重新生成最小化补丁
pwsh tools/scripts/regenerate_minimal_patch.ps1

# 步骤 4：测试新补丁
pwsh tools/scripts/test_minimal_patch.ps1

# 步骤 5：对比新旧补丁
pwsh tools/scripts/diff_patch.ps1 -Mode patches
```

## 🎨 颜色编码

所有工具使用统一的颜色编码：

- 🟢 **绿色** - 新增的文件/代码
- 🟡 **黄色** - 修改的文件/警告
- 🔴 **红色** - 删除的文件/代码
- 🔵 **青色** - 标题和分类
- ⚪ **灰色** - 普通内容和路径

## 💡 实用技巧

### 导出报告

```powershell
# 生成补丁摘要报告
pwsh tools/scripts/view_patch.ps1 -Mode stats > patch_report.txt

# 生成差异报告
pwsh tools/scripts/diff_patch.ps1 -Mode patches > comparison.txt

# 导出文件列表
pwsh tools/scripts/view_patch.ps1 -Mode files > files.txt
```

### 搜索和过滤

```powershell
# 查找所有头文件
pwsh tools/scripts/view_patch.ps1 -Mode files | Select-String "\.h$"

# 查找特定目录的文件
pwsh tools/scripts/view_patch.ps1 -Mode files | Select-String "narrowphase"

# 统计新增文件
pwsh tools/scripts/view_patch.ps1 -Mode files | Select-String "\[\+\]"
```

### 与 Git 配合

```powershell
# 查看补丁状态
cd external/fcl-source
git status

# 查看具体差异
git diff HEAD -- include/fcl/logging.h

# 查看统计
git diff HEAD --stat
```

## 🆘 故障排除

### 问题：工具无法运行

```powershell
# 检查 PowerShell 执行策略
Get-ExecutionPolicy

# 临时允许脚本执行
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

### 问题：VS Code 无法打开

```powershell
# 安装 code 命令
# 在 VS Code 中: Ctrl+Shift+P -> "Shell Command: Install 'code' command in PATH"

# 验证
code --version
```

### 问题：Git 锁文件错误

```powershell
# 移除锁文件
rm .git/index.lock
```

### 问题：补丁未应用

```powershell
# 应用补丁
pwsh tools/scripts/apply_fcl_patch.ps1

# 验证
pwsh tools/scripts/diff_patch.ps1
```

## 📚 详细文档

- `README_view_patch.md` - 查看工具完整文档
- `README_diff_patch.md` - 对比工具完整文档
- 各工具内置帮助：使用 `-?` 参数查看

## 🔗 相关文件

- `patches/fcl-kernel-mode-minimal.patch` - 最小化补丁（290KB）
- `patches/fcl-kernel-mode.patch` - 原始补丁（43MB）
- `external/fcl-source/` - FCL 源码目录

## ⚡ 性能优化

- ✅ 优先使用 `view_patch.ps1` 快速浏览
- ✅ 使用 `diff_patch.ps1 -Mode file` 查看单个文件
- ✅ 大型对比优先使用 VS Code
- ✅ 使用 `-Quiet` 参数减少输出

## 🎯 最佳实践

1. **构建前必做**
   ```powershell
   pwsh tools/scripts/apply_fcl_patch.ps1
   ```

2. **代码审查**
   ```powershell
   pwsh tools/scripts/view_patch.ps1 -Mode category
   pwsh tools/scripts/diff_patch.ps1 -Mode patches
   ```

3. **定期验证**
   ```powershell
   pwsh tools/scripts/test_minimal_patch.ps1
   ```

4. **清理状态**
   ```powershell
   pwsh tools/scripts/apply_fcl_patch.ps1 -Restore
   ```

---

**提示：** 所有工具都支持 `-?` 参数查看帮助信息
