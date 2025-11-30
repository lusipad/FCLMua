# 贡献指南

感谢你考虑为 FCL+Musa 做出贡献！我们欢迎各种形式的贡献。

## 📋 目录

- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
- [开发环境设置](#开发环境设置)
- [代码规范](#代码规范)
- [提交流程](#提交流程)
- [报告Bug](#报告bug)
- [功能请求](#功能请求)

## 🤝 行为准则

本项目遵循开源社区的基本准则：

- 保持尊重和包容
- 接受建设性批评
- 专注于对社区最有利的事情
- 对其他社区成员表示同理心

## 🛠️ 如何贡献

### 贡献类型

我们欢迎以下类型的贡献：

- 🐛 **Bug 修复** - 修复已知问题
- ✨ **新功能** - 添加新的功能
- 📝 **文档改进** - 改进文档质量
- 🎨 **代码重构** - 优化代码结构
- ⚡ **性能优化** - 提升运行效率
- ✅ **测试** - 增加测试覆盖率

### 贡献前的准备

1. **Fork 仓库** - 在 GitHub 上 Fork 本项目
2. **克隆仓库** - 克隆到本地
   ```bash
   git clone --recursive https://github.com/your-username/FCL+Musa.git
   cd FCL+Musa
   ```
3. **创建分支** - 为你的改动创建一个新分支
   ```bash
   git checkout -b feature/your-feature-name
   # 或
   git checkout -b fix/your-bug-fix
   ```

## 💻 开发环境设置

### 系统要求

- **操作系统**: Windows 10/11
- **编译器**: Visual Studio 2022 with C++ Desktop Development workload
- **CMake**: 3.24 或更高版本
- **PowerShell**: 7.0 或更高版本
- **WDK**: Windows Driver Kit 10.0.22621.0+ (仅内核态开发需要)

### 安装依赖

```powershell
# 检查环境
pwsh build.ps1
# 选择 "4. Check Env"

# 如果缺少 Musa.Runtime，会自动下载
```

### 构建项目

```powershell
# 使用交互式菜单
pwsh build.ps1

# 或直接构建
pwsh tools/build/build-tasks.ps1 -Task All
```

### 运行测试

```powershell
# 运行所有测试
pwsh tools/scripts/run_all_tests.ps1

# 运行特定测试
pwsh tools/build/test-tasks.ps1 -Task R3-Demo
```

## 📖 代码规范

### C++ 代码风格

- **缩进**: 4个空格（不使用Tab）
- **命名约定**:
  - 类型: `PascalCase` (如 `CollisionObject`)
  - 函数: `PascalCase` (如 `FclCreateGeometry`)
  - 变量: `camelCase` (如 `sphereHandle`)
  - 常量: `UPPER_SNAKE_CASE` (如 `FCL_MAX_SIZE`)
- **括号风格**: K&R 变体（开括号同行）
  ```cpp
  if (condition) {
      // code
  }
  ```
- **注释**: 使用Doxygen风格的注释
  ```cpp
  /**
   * @brief Brief description
   * @param paramName Parameter description
   * @return Return value description
   */
  NTSTATUS FunctionName(PARAM_TYPE paramName);
  ```

### PowerShell 代码风格

- **缩进**: 4个空格
- **命名**: PascalCase for functions, camelCase for variables
- **注释**: 使用 `<# ... #>` 块注释和 `#` 行注释

### 文档规范

- Markdown 文件使用中文或英文
- 代码示例应可直接运行
- 包含必要的上下文说明

## 📝 提交流程

### 1. 提交代码

```bash
# 添加改动
git add .

# 提交（使用有意义的提交信息）
git commit -m "feat: add support for capsule geometry"
# 或
git commit -m "fix: resolve memory leak in geometry manager"
```

### 提交信息格式

我们使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <subject>

<body>

<footer>
```

**类型 (type)**:
- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式调整
- `refactor`: 代码重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具链相关

**范围 (scope)** (可选):
- `kernel`: 内核态代码
- `userlib`: 用户态库
- `build`: 构建系统
- `docs`: 文档
- `samples`: 示例代码

**示例**:
```
feat(kernel): add capsule-capsule collision detection

Implements GJK-based collision detection for capsule geometries.
Includes unit tests and performance benchmarks.

Closes #123
```

### 2. 推送到 Fork

```bash
git push origin feature/your-feature-name
```

### 3. 创建 Pull Request

1. 访问 GitHub 上的原仓库
2. 点击 "New Pull Request"
3. 选择你的 Fork 和分支
4. 填写 PR 描述：
   - **标题**: 简洁描述改动
   - **描述**: 详细说明改动内容、原因和测试方法
   - **关联 Issue**: 使用 `Closes #issue-number`

### PR 检查清单

在提交 PR 前，请确认：

- [ ] 代码遵循项目代码规范
- [ ] 添加了适当的测试
- [ ] 所有测试通过
- [ ] 更新了相关文档
- [ ] 提交信息遵循 Conventional Commits
- [ ] 代码没有引入新的警告
- [ ] 自己review过代码

## 🐛 报告 Bug

### 提交 Bug 前

1. **搜索已有 Issue** - 确保 Bug 尚未被报告
2. **确认可重现** - 在干净的环境中验证
3. **收集信息** - 准备详细的复现步骤

### Bug 报告模板

```markdown
**描述**
简明扼要地描述 Bug。

**复现步骤**
1. 执行 '...'
2. 运行 '....'
3. 看到错误

**预期行为**
描述你期望发生什么。

**实际行为**
描述实际发生了什么。

**环境信息**
- OS: [e.g. Windows 11 22H2]
- 编译器: [e.g. MSVC 19.44]
- CMake版本: [e.g. 3.28]
- WDK版本 (如适用): [e.g. 10.0.26100.0]

**额外上下文**
添加任何其他相关信息、截图或日志。

**可能的解决方案**
(可选) 如果你有想法，请说明。
```

## ✨ 功能请求

### 提交功能请求

使用 GitHub Issues，标记为 `enhancement`。

### 功能请求模板

```markdown
**功能描述**
清楚简洁地描述你想要的功能。

**使用场景**
描述此功能解决的问题或需求。

**建议的实现**
(可选) 描述你希望如何实现此功能。

**替代方案**
(可选) 描述你考虑过的替代方案。

**额外上下文**
添加任何其他相关信息或截图。
```

## 🔍 代码审查流程

所有提交都将经过代码审查：

1. **自动检查**: CI 构建和测试
2. **代码审查**: 至少一位维护者审查
3. **修改**: 根据反馈进行调整
4. **合并**: 审查通过后合并

### 审查标准

- 代码质量和可维护性
- 测试覆盖率
- 文档完整性
- 性能影响
- API 兼容性

## 📚 附加资源

- [项目架构文档](docs/architecture.md)
- [API 参考](docs/api.md)
- [构建系统文档](BUILD_SYSTEM.md)
- [CPM 集成指南](docs/cpm_integration.md)

## 💬 获取帮助

如果你有任何问题：

- 💬 [GitHub Discussions](https://github.com/yourname/FCL+Musa/discussions) - 一般讨论
- 🐛 [GitHub Issues](https://github.com/yourname/FCL+Musa/issues) - Bug 报告和功能请求

## 🙏 致谢

感谢所有贡献者！你们的贡献让这个项目更加出色。

---

再次感谢你的贡献！ 🎉
