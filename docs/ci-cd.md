# CI/CD 文档

## GitHub Actions 工作流

本项目使用 GitHub Actions 实现 Windows 环境下的自动化构建、打包和测试。

### 工作流概览

工作流文件位置: `.github/workflows/windows-build.yml`

**触发条件:**
- 推送到 `main` 或 `master` 分支
- 推送到以 `claude/` 开头的分支
- 针对 `main` 或 `master` 分支的 Pull Request
- 手动触发 (通过 GitHub Actions 界面)

### 构建环境

- **运行器:** Windows Server 2022
- **Visual Studio:** 2022 (17.x)
- **WDK 版本:** 10.0.26100.0
- **构建配置:** Debug (可通过环境变量修改)
- **平台:** x64

### 工作流步骤

#### 1. 环境准备
- 检出代码仓库 (包含子模块)
- 设置 MSBuild 工具
- 检测 Visual Studio 安装路径

#### 2. 安装 Windows Driver Kit
- 检查 WDK 是否已安装
- 如未安装,自动下载并安装 WDK 10.0.26100.0
- 验证 WDK 安装是否成功 (检查 `ntddk.h` 是否存在)

#### 3. 构建驱动程序
- 初始化 Visual Studio 开发环境
- 配置 WDK 环境变量 (INCLUDE, LIB, PATH)
- 清理之前的构建输出
- 使用 MSBuild 构建 `FclMusaDriver.sln`
- 生成产物:
  - `FclMusaDriver.sys` - 驱动文件
  - `FclMusaDriver.pdb` - 符号文件

#### 4. 构建用户态示例程序
- 初始化 Visual Studio 开发环境
- 使用 cl.exe 编译 `fcl_demo.cpp`
- 复制示例资源文件 (assets, scenes)
- 生成产物:
  - `fcl_demo.exe` - 示例程序
  - 资源文件目录

#### 5. 上传构建产物
工作流会生成以下构建产物(Artifacts):

1. **驱动产物** (`fclmusa-driver-x64-Debug`)
   - FclMusaDriver.sys
   - FclMusaDriver.pdb
   - 保留期: 30 天

2. **示例程序产物** (`fcl-demo-x64-Debug`)
   - fcl_demo.exe
   - assets/ (资源文件)
   - scenes/ (场景文件)
   - 保留期: 30 天

3. **发布包** (`fclmusa-release-x64-Debug`)
   - 包含上述所有文件的 ZIP 压缩包
   - 包含文档 (README.md, docs/)
   - 保留期: 90 天

### 使用方法

#### 查看工作流运行状态

1. 访问仓库的 **Actions** 标签页
2. 选择 **Windows Build and Test** 工作流
3. 查看最近的运行记录

#### 下载构建产物

1. 进入某次工作流运行详情
2. 滚动到页面底部的 **Artifacts** 部分
3. 点击相应的产物名称进行下载

#### 手动触发构建

1. 访问 **Actions** 标签页
2. 选择 **Windows Build and Test** 工作流
3. 点击 **Run workflow** 按钮
4. 选择要构建的分支
5. 点击绿色的 **Run workflow** 按钮

### 自定义配置

如需修改构建配置,可以编辑工作流文件中的环境变量:

```yaml
env:
  WDK_VERSION: '10.0.26100.0'      # WDK 版本
  BUILD_CONFIGURATION: Debug        # 构建配置: Debug 或 Release
  BUILD_PLATFORM: x64               # 构建平台: x64 或 Win32
```

### 测试集成 (待实现)

工作流中包含一个 `test` 作业,目前默认禁用 (`if: false`)。待测试基础设施就绪后,可以启用此作业来:

- 自动加载驱动
- 运行 `fcl-self-test.ps1` 自检脚本
- 验证测试结果
- 生成测试报告

要启用测试,将 `test` 作业中的 `if: false` 改为 `if: true`:

```yaml
test:
  name: Run Tests
  needs: build
  runs-on: windows-2022
  if: true  # 启用测试
```

### 故障排查

#### WDK 安装失败

如果 WDK 安装失败,可能是由于:
- 网络问题导致下载失败
- 磁盘空间不足
- WDK 版本链接已过期

**解决方法:**
- 检查 WDK 下载链接是否有效
- 更新 `WDK_VERSION` 环境变量到可用版本
- 考虑使用 GitHub Actions 缓存来缓存 WDK 安装

#### 构建失败

如果驱动或示例程序构建失败:
- 查看构建日志中的详细错误信息
- 确认代码在本地 Windows 环境下可以正常构建
- 检查是否缺少必要的依赖项

#### 产物上传失败

如果产物上传失败 (`if-no-files-found: error`):
- 检查构建步骤是否成功完成
- 确认产物路径是否正确
- 查看 `List build artifacts` 步骤的输出

### 性能优化建议

#### 1. 缓存 WDK 安装

可以使用 GitHub Actions 缓存来避免每次都重新下载 WDK:

```yaml
- name: Cache WDK
  uses: actions/cache@v4
  with:
    path: C:\Program Files (x86)\Windows Kits\10
    key: wdk-${{ env.WDK_VERSION }}
```

#### 2. 并行构建

如果有多个配置需要构建 (例如 Debug 和 Release),可以使用矩阵策略:

```yaml
strategy:
  matrix:
    configuration: [Debug, Release]

env:
  BUILD_CONFIGURATION: ${{ matrix.configuration }}
```

#### 3. 增量构建

对于大型项目,可以启用 MSBuild 的增量构建功能,减少构建时间。

### 相关文档

- [部署文档](deployment.md) - 驱动安装和签名
- [测试文档](testing.md) - 自检和验证步骤
- [使用文档](usage.md) - 快速使用指南

### 参考链接

- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [Windows Driver Kit 下载](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- [MSBuild 参考](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild)
