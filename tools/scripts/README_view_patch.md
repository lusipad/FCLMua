# 补丁查看工具使用指南

## 快速开始

```powershell
# 查看补丁摘要（默认）
pwsh tools/scripts/view_patch.ps1

# 按类型分类显示
pwsh tools/scripts/view_patch.ps1 -Mode category

# 查看详细统计
pwsh tools/scripts/view_patch.ps1 -Mode stats

# 只显示文件列表
pwsh tools/scripts/view_patch.ps1 -Mode files

# 查看特定文件的修改
pwsh tools/scripts/view_patch.ps1 -File "include/fcl/logging.h"
```

## 所有模式

### 1. `summary` - 摘要模式（默认）

显示：
- 文件总数、新增、修改、删除统计
- 代码行数变更统计
- 修改最多的前 10 个文件

```powershell
pwsh tools/scripts/view_patch.ps1
# 或
pwsh tools/scripts/view_patch.ps1 -Mode summary
```

### 2. `category` - 分类模式

按文件类型分组显示：
- Headers (include/)
- Source (src/)
- Tests
- Build System
- Documentation
- Other

```powershell
pwsh tools/scripts/view_patch.ps1 -Mode category
```

### 3. `stats` - 统计模式

显示所有文件的详细统计信息（增加/删除行数）

```powershell
pwsh tools/scripts/view_patch.ps1 -Mode stats
```

### 4. `files` - 文件列表模式

仅显示文件路径列表，标记状态：
- `[+]` 新增文件
- `[M]` 修改文件
- `[-]` 删除文件

```powershell
pwsh tools/scripts/view_patch.ps1 -Mode files
```

### 5. 查看特定文件

查看某个文件的完整修改内容（带语法高亮）

```powershell
# 精确匹配
pwsh tools/scripts/view_patch.ps1 -File "include/fcl/logging.h"

# 模糊匹配
pwsh tools/scripts/view_patch.ps1 -File "logging.h"
```

## 实用技巧

### 导出文件列表

```powershell
# 导出所有文件列表到文本
pwsh tools/scripts/view_patch.ps1 -Mode files > patch_files.txt

# 只导出新增的文件
pwsh tools/scripts/view_patch.ps1 -Mode files | Select-String "\[\+\]" > new_files.txt
```

### 搜索特定文件

```powershell
# 查找所有头文件
pwsh tools/scripts/view_patch.ps1 -Mode files | Select-String "\.h$"

# 查找特定目录的文件
pwsh tools/scripts/view_patch.ps1 -Mode files | Select-String "narrowphase"
```

### 统计分析

```powershell
# 查看修改最多的文件
pwsh tools/scripts/view_patch.ps1 -Mode summary

# 查看每个类别的文件数量
pwsh tools/scripts/view_patch.ps1 -Mode category
```

## 选项参数

### `-Mode <模式>`

可选值：
- `summary` - 摘要（默认）
- `category` - 按类型分类
- `stats` - 详细统计
- `files` - 文件列表
- `diff` - 完整 diff（分页显示）

### `-File <文件路径>`

查看特定文件的修改，支持完整路径或部分匹配

### `-UseMinimal`

使用最小化补丁（默认：`$true`）

若要查看原始大补丁：
```powershell
pwsh tools/scripts/view_patch.ps1 -UseMinimal:$false
```

## 常见使用场景

### 场景 1：快速了解补丁内容

```powershell
# 1. 查看摘要
pwsh tools/scripts/view_patch.ps1

# 2. 按类型查看
pwsh tools/scripts/view_patch.ps1 -Mode category
```

### 场景 2：代码审查

```powershell
# 1. 查看所有文件列表
pwsh tools/scripts/view_patch.ps1 -Mode files

# 2. 逐个查看重要文件
pwsh tools/scripts/view_patch.ps1 -File "include/fcl/logging.h"
pwsh tools/scripts/view_patch.ps1 -File "src/common/detail/profiler.cpp"
```

### 场景 3：生成报告

```powershell
# 生成补丁分析报告
pwsh tools/scripts/view_patch.ps1 -Mode stats > patch_report.txt
```

## 颜色编码

脚本使用颜色区分不同类型的修改：

- 🟢 **绿色** - 新增文件/新增代码行
- 🟡 **黄色** - 修改的文件
- 🔴 **红色** - 删除的文件/删除的代码行
- 🔵 **青色** - 标题和分类
- ⚪ **灰色** - 普通内容和路径

## 相关工具

- `apply_fcl_patch.ps1` - 应用/恢复补丁
- `regenerate_minimal_patch.ps1` - 重新生成最小化补丁
- `test_minimal_patch.ps1` - 测试补丁有效性

## 示例输出

### 摘要模式输出

```
============================================
  补丁统计: fcl-kernel-mode-minimal.patch
============================================

文件变更：
  总文件数:     64
  新增文件:     5
  修改文件:     59
  删除文件:     0

代码变更：
  新增行数:     +3879
  删除行数:     -3026
  净变更:       +853

最近修改的文件（前 10 个）：
  [修改] include/fcl/narrowphase/detail/traversal/collision/intersect-inl.h (+1146/-1146)
  ...
```
