# 工具脚本使用指南

## 统一入口：build.ps1

**推荐使用方式**：所有常规开发任务都通过 `build.ps1` 完成

```powershell
pwsh build.ps1
```

### 菜单功能：

1. **Build** - 编译项目
   - R0 Debug/Release（内核驱动）
   - R3 Lib Debug/Release（用户态库）
   - R3 Demo Debug/Release
   - CLI Demo
   - GUI Demo
   - All（编译所有）

2. **Test** - 运行测试
   - R0 Demo（驱动测试）
   - R3 Demo（用户态测试）
   - GUI Demo（图形界面测试）
   - **All Tests（完整测试套件）** ✨

3. **Doc** - 生成文档
   - Doxygen API 文档

4. **Check Env** - 检查环境
   - 验证所有必需工具和依赖

5. **Check Upstream** - 检查上游更新
   - FCL、Eigen、libccd 等依赖库

6. **FCL Patch** - 管理补丁
   - 应用补丁
   - 恢复到上游版本

---

## 独立工具脚本

### 驱动管理
```powershell
# 安装/卸载/启动/停止驱动
pwsh tools\scripts\manage_driver.ps1 -Action [Install|Uninstall|Start|Stop]
```

### 打包发布
```powershell
# 创建发布包
pwsh tools\scripts\package_bundle.ps1
```

### Hyper-V 实验环境
```powershell
# 设置 Hyper-V 测试虚拟机
pwsh tools\scripts\setup-hyperv-lab.ps1
```

### 补丁工具

**查看补丁**：
```powershell
pwsh tools\scripts\view_patch.ps1 -PatchFile patches\fcl-kernel-mode.patch
```

**生成差异补丁**：
```powershell
pwsh tools\scripts\diff_patch.ps1
```

**重新生成最小补丁**：
```powershell
pwsh tools\scripts\regenerate_minimal_patch.ps1
```

**验证上游源文件**：
```powershell
pwsh tools\scripts\verify_upstream_sources.ps1
```

**测试最小补丁**：
```powershell
pwsh tools\scripts\test_minimal_patch.ps1
```

---

## 内部使用脚本

以下脚本由构建系统自动调用，通常不需要手动执行：

- `setup_dependencies.ps1` - 安装 Musa.Runtime
- `restore_kernel_packages.ps1` - 恢复 NuGet 包
- `sign_driver.ps1` - 驱动签名
- `install_wdk.ps1` - WDK 安装（CI 使用）

---

## 快速开始示例

### 1. 首次构建
```powershell
# 启动交互式菜单
pwsh build.ps1

# 选择: 1 (Build) -> 9 (All)
```

### 2. 运行完整测试
```powershell
# 启动交互式菜单
pwsh build.ps1

# 选择: 2 (Test) -> 4 (All Tests)
```

### 3. 安装驱动测试
```powershell
# 构建驱动
pwsh build.ps1
# 选择: 1 (Build) -> 2 (R0 Release)

# 安装驱动
pwsh tools\scripts\manage_driver.ps1 -Action Install

# 启动驱动
pwsh tools\scripts\manage_driver.ps1 -Action Start
```

### 4. 创建发布包
```powershell
# 构建所有目标
pwsh build.ps1
# 选择: 1 (Build) -> 9 (All)

# 打包
pwsh tools\scripts\package_bundle.ps1
```

---

## 故障排除

### 环境检查
```powershell
pwsh build.ps1
# 选择: 4 (Check Env)
```

### 查看 WDK 安装状态
```powershell
# 通过 Check Env 菜单项查看
```

### 重新安装依赖
```powershell
# 删除 external/Musa.Runtime/Publish
# 运行 build.ps1，依赖会自动重新安装
```

---

## 文件组织

```
FCL+Musa/
├── build.ps1                  # ✨ 统一入口
├── tools/
│   ├── build/                 # 构建任务（build.ps1 调用）
│   │   ├── common.psm1
│   │   ├── build-tasks.ps1
│   │   ├── test-tasks.ps1
│   │   ├── doc-tasks.ps1
│   │   ├── check-env.ps1
│   │   └── check-upstream.ps1
│   └── scripts/               # 独立工具脚本
│       ├── manage_driver.ps1
│       ├── package_bundle.ps1
│       ├── setup-hyperv-lab.ps1
│       ├── run_all_tests.ps1
│       ├── apply_fcl_patch.ps1
│       ├── diff_patch.ps1
│       ├── view_patch.ps1
│       ├── regenerate_minimal_patch.ps1
│       ├── verify_upstream_sources.ps1
│       ├── test_minimal_patch.ps1
│       ├── setup_dependencies.ps1      # 内部使用
│       ├── restore_kernel_packages.ps1 # 内部使用
│       ├── sign_driver.ps1             # 内部使用
│       └── install_wdk.ps1             # CI 使用
└── ...
```
