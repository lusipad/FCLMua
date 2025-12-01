# 补丁 Diff 对比工具使用指南

## 快速开始

```powershell
# 查看应用补丁前后的差异
pwsh tools/scripts/diff_patch.ps1

# 对比原始补丁和最小化补丁
pwsh tools/scripts/diff_patch.ps1 -Mode patches

# 对比特定文件
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/logging.h"

# 在 VS Code 中打开对比
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/logging.h" -Tool vscode
```

## 对比模式

### 1. `before-after` - 应用前后对比（默认）

查看应用补丁前后的所有差异

```powershell
# 查看所有差异
pwsh tools/scripts/diff_patch.ps1

# 查看特定文件的差异
pwsh tools/scripts/diff_patch.ps1 -File "src/common/detail/profiler.cpp"
```

**显示内容：**
- 统计信息（修改的文件数、行数）
- 详细的 diff 内容（分页显示）
- 彩色输出（+ 绿色，- 红色）

### 2. `patches` - 补丁文件对比

对比原始 43MB 补丁和最小化 290KB 补丁的差异

```powershell
pwsh tools/scripts/diff_patch.ps1 -Mode patches
```

**显示内容：**
- 文件大小对比
- 文件数量对比（143 vs 64 个）
- 被排除的文件列表（按类型分组）

### 3. `file` - 单文件对比

查看特定文件的详细修改

```powershell
pwsh tools/scripts/diff_patch.ps1 -Mode file -File "include/fcl/logging.h"
```

**功能：**
- 显示文件的完整 diff
- 支持在外部工具中打开
- 自动应用补丁（如果需要）

### 4. `side-by-side` - 交互式并排对比

交互式选择文件进行对比

```powershell
pwsh tools/scripts/diff_patch.ps1 -Mode side-by-side
```

**工作流程：**
1. 列出所有修改的文件
2. 选择要查看的文件编号
3. 显示该文件的 diff

## 查看工具

### Git Diff（默认）

使用 git 内置的 diff 功能，彩色输出

```powershell
pwsh tools/scripts/diff_patch.ps1 -Tool git
```

**优点：**
- ✅ 内置工具，无需安装
- ✅ 彩色高亮
- ✅ 终端内查看

### VS Code

在 VS Code 中打开并排对比

```powershell
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/logging.h" -Tool vscode
```

**优点：**
- ✅ 可视化界面
- ✅ 并排对比
- ✅ 可编辑
- ✅ 语法高亮

**要求：** 已安装 VS Code 并将 `code` 命令加入 PATH

### Beyond Compare（计划支持）

```powershell
pwsh tools/scripts/diff_patch.ps1 -Tool beyond
```

### Meld（计划支持）

```powershell
pwsh tools/scripts/diff_patch.ps1 -Tool meld
```

## 常见使用场景

### 场景 1：快速了解补丁修改了什么

```powershell
# 1. 查看补丁文件对比
pwsh tools/scripts/diff_patch.ps1 -Mode patches

# 2. 查看摘要
pwsh tools/scripts/view_patch.ps1

# 3. 查看分类
pwsh tools/scripts/view_patch.ps1 -Mode category
```

### 场景 2：审查特定文件的修改

```powershell
# 1. 查看文件在补丁中的修改
pwsh tools/scripts/view_patch.ps1 -File "include/fcl/logging.h"

# 2. 在 VS Code 中对比原始版本
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/logging.h" -Tool vscode
```

### 场景 3：验证补丁是否正确应用

```powershell
# 1. 应用补丁
pwsh tools/scripts/apply_fcl_patch.ps1

# 2. 查看应用后的差异
pwsh tools/scripts/diff_patch.ps1

# 3. 对比特定文件
pwsh tools/scripts/diff_patch.ps1 -File "src/common/detail/profiler.cpp"
```

### 场景 4：生成对比报告

```powershell
# 生成补丁对比报告
pwsh tools/scripts/diff_patch.ps1 -Mode patches > patch_comparison.txt

# 生成文件差异报告
pwsh tools/scripts/diff_patch.ps1 -Mode before-after > changes.diff
```

## 工作流程示例

### 完整的补丁审查流程

```powershell
# 步骤 1：了解补丁概况
pwsh tools/scripts/view_patch.ps1 -Mode summary

# 步骤 2：查看补丁优化效果
pwsh tools/scripts/diff_patch.ps1 -Mode patches

# 步骤 3：按类型查看修改
pwsh tools/scripts/view_patch.ps1 -Mode category

# 步骤 4：审查重要文件
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/logging.h" -Tool vscode

# 步骤 5：查看所有差异
pwsh tools/scripts/diff_patch.ps1 -Mode side-by-side
```

## 输出说明

### 颜色编码

终端输出使用颜色区分：
- 🟢 **绿色** (+) - 新增的内容
- 🔴 **红色** (-) - 删除的内容
- 🔵 **青色** - 文件路径和标题
- 🟡 **黄色** - 警告和提示
- ⚪ **白色/灰色** - 普通文本

### Git Diff 格式说明

```diff
diff --git a/include/fcl/logging.h b/include/fcl/logging.h
new file mode 100644
index 0000000..904694b
--- /dev/null
+++ b/include/fcl/logging.h
@@ -0,0 +1,102 @@
+#pragma once
+
+#include <ostream>
```

- `diff --git` - 对比的文件
- `new file mode` - 新增文件
- `@@ -0,0 +1,102 @@` - 行号范围
- `+` 开头 - 新增的行
- `-` 开头 - 删除的行

## 与其他工具配合

### 与 view_patch.ps1 配合

```powershell
# 1. 使用 view_patch 查看文件列表
pwsh tools/scripts/view_patch.ps1 -Mode files > files.txt

# 2. 从列表中选择文件用 diff_patch 查看
pwsh tools/scripts/diff_patch.ps1 -File "include/fcl/common/types.h"
```

### 与 Git 命令配合

```powershell
# 1. 查看补丁差异
pwsh tools/scripts/diff_patch.ps1

# 2. 使用 git 命令查看具体细节
cd external/fcl-source
git diff HEAD --stat
git diff HEAD -- include/fcl/logging.h
```

## 故障排除

### 问题：显示 "文件未修改"

**原因：** 补丁尚未应用

**解决：**
```powershell
pwsh tools/scripts/apply_fcl_patch.ps1
```

### 问题：VS Code 无法打开

**原因：** `code` 命令未在 PATH 中

**解决：**
1. 确保已安装 VS Code
2. 在 VS Code 中按 Ctrl+Shift+P
3. 输入 "Shell Command: Install 'code' command in PATH"
4. 重启终端

### 问题：文件不在补丁中

**原因：** 指定的文件不包含在最小化补丁中

**解决：**
```powershell
# 查看补丁中的所有文件
pwsh tools/scripts/view_patch.ps1 -Mode files

# 或使用原始补丁
pwsh tools/scripts/diff_patch.ps1 -File "your-file.cpp" # 会自动切换
```

## 性能提示

- ✅ 对于大文件，使用 `-Mode file` 只查看特定文件
- ✅ 使用 `view_patch.ps1` 先浏览，再用 `diff_patch.ps1` 详细查看
- ✅ 对于视觉对比，优先使用 `-Tool vscode`

## 相关工具

- `view_patch.ps1` - 查看补丁摘要和统计
- `apply_fcl_patch.ps1` - 应用/恢复补丁
- `regenerate_minimal_patch.ps1` - 重新生成最小化补丁
- `test_minimal_patch.ps1` - 测试补丁有效性

## 快速参考卡

```powershell
# 对比补丁文件
diff_patch.ps1 -Mode patches

# 查看应用前后
diff_patch.ps1

# VS Code 对比
diff_patch.ps1 -File "file.h" -Tool vscode

# 交互式选择
diff_patch.ps1 -Mode side-by-side
```
