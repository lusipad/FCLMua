# GitHub 发布检查清单

在推送到 GitHub 前，请确认以下事项：

## ✅ 文档检查

- [x] **README.md** - 主文档创建完成
  - 项目简介
  - 快速开始指南
  - 安装方法
  - 使用示例
  - CPM集成说明
  - 项目结构
  - 许可证信息

- [x] **LICENSE** - BSD 3-Clause 许可证
  - 主项目许可证
  - 依赖项许可证说明

- [x] **CONTRIBUTING.md** - 贡献指南
  - 开发环境设置
  - 代码规范
  - 提交流程
  - Bug报告模板
  - 功能请求模板

- [x] **.gitignore** - Git忽略规则
  - 构建产物
  - IDE配置
  - 临时文件
  - 系统文件

- [x] **BUILD_SYSTEM.md** - 构建系统文档

## 📋 代码检查

### 必须修复

- [ ] **移除敏感信息**
  - 个人路径
  - 凭证信息
  - 测试证书私钥

- [ ] **确认子模块状态**
  ```bash
  git submodule status
  # 确保所有子模块都已正确初始化
  ```

- [ ] **清理构建产物**
  ```bash
  # 确保以下目录不会被提交
  - build/
  - dist/
  - *.exe, *.dll, *.sys
  ```

### 建议优化

- [ ] 运行所有测试确保通过
- [ ] 检查代码中的TODO/FIXME
- [ ] 更新CHANGELOG.md（如果有）

## 🏷️ GitHub 仓库设置

### 创建仓库后

1. **设置仓库描述**
   ```
   Windows kernel-mode and user-mode port of FCL (Flexible Collision Library) with CMake/CPM support
   ```

2. **添加主题标签 (Topics)**
   ```
   collision-detection
   fcl
   windows-driver
   kernel-mode
   cmake
   cpp
   cpm
   eigen
   geometry
   ```

3. **启用 GitHub Features**
   - [ ] Issues
   - [ ] Discussions (可选)
   - [ ] Wiki (可选)
   - [ ] Projects (可选)

4. **设置分支保护规则** (main分支)
   - [ ] Require pull request reviews
   - [ ] Require status checks to pass
   - [ ] Restrict who can push

## 📝 首次推送步骤

```bash
# 1. 添加远程仓库
git remote add origin https://github.com/lusipad/FCLMua.git

# 2. 查看当前状态
git status

# 3. 暂存新文件
git add README.md LICENSE CONTRIBUTING.md .gitignore

# 4. 提交
git commit -m "docs: add project documentation for GitHub release

- Add comprehensive README with quick start guide
- Add BSD 3-Clause LICENSE with third-party notices
- Add CONTRIBUTING.md with development guidelines
- Add .gitignore for build artifacts and temp files"

# 5. 推送到 GitHub
git push -u origin feature/reorg

# 6. 创建 Pull Request 合并到 main
# 或直接推送到 main (如果你有权限)
git checkout main
git merge feature/reorg
git push -u origin main
```

## 🎉 发布后的工作

### 创建首个 Release

1. 前往 GitHub Releases
2. 点击 "Create a new release"
3. 创建标签: `v0.1.0`
4. 填写发布说明:

```markdown
# FCL+Musa v0.1.0

首个公开发布版本！

## ✨ 特性

- ✅ FCL移植到Windows内核态
- ✅ 用户态库支持（无需WDK）
- ✅ CPM集成支持
- ✅ 完整的示例项目
- ✅ 交互式构建系统

## 📦 支持的几何体

- Sphere (球体)
- OBB (方向包围盒)
- Mesh (网格)

## 📋 依赖

- Eigen 3.x (内置)
- FCL (内置)
- libccd (内置)
- Musa.Runtime (自动下载)

## 🚀 快速开始

查看 [README.md](README.md) 获取详细安装和使用指南。

## 📝 已知问题

- 第三方库编译时存在警告（不影响功能）

## 🙏 致谢

感谢 FCL、Eigen 和 libccd 项目的贡献者！
```

### 添加 Badges 到 README

在 GitHub 仓库创建后，更新 README.md 中的 badges URL。

### 设置 GitHub Actions (可选)

创建 `.github/workflows/build.yml` 用于 CI/CD。

## ⚠️ 注意事项

1. **不要推送的内容**:
   - 个人配置文件
   - 构建产物
   - 测试证书私钥
   - 大型二进制文件（除非使用 Git LFS）

2. **Git LFS 考虑**:
   - 如果有大型测试资产，考虑使用 Git LFS
   - nuget.exe (8MB) 可能需要 LFS

3. **安全检查**:
   ```bash
   # 搜索可能的敏感信息
   grep -r "password\|secret\|token\|key" --include="*.cpp" --include="*.h" --include="*.ps1"
   ```

## 📞 需要帮助？

如有疑问，请参考:
- [GitHub Docs](https://docs.github.com/)
- [Git 文档](https://git-scm.com/doc)

---

**准备就绪！** 🚀 祝发布顺利！
