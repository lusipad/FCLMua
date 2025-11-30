# Repository Guidelines

## 项目结构与模块组织
FCL+Musa 将运行逻辑划分为内核态 (R0) 与用户态 (R3)。核心目录：`kernel/` (碰撞核心与驱动包装，含 `kernel/tests/` 的驱动验证)、`samples/` (CLI/GUI/R3 示例)、`external/` (Eigen、libccd、上游 FCL、Musa.Runtime 的镜像)、`tests/` (面向 SDK 的回归套件) 以及 `docs/` 和 `openspec/` (设计与 API 说明)。构建脚本集中在 `tools/`，生成物落在 `build/` 与 `dist/`，若需手动引擎配置参考 `BUILD_SYSTEM.md`。

## 构建、测试与开发命令
- `pwsh build.ps1`：交互式入口，可执行 Build/Test/Doc/Check Env。
- `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`：生成可在 VS 中调试的工程。
- `cmake --build build --config Release`：按所选配置编译 R3 库、示例与驱动。
- `pwsh tools/build/build-tasks.ps1 -Task All`：CI 使用的批量构建流程。
- `pwsh tools/scripts/run_all_tests.ps1 [-Filter <模块>]`：一次性运行用户态与内核态测试矩阵。

## 代码风格与命名
C++ 统一使用 C++17、四空格缩进、K&R 括号。类型/函数遵循 `PascalCase`，局部变量用 `camelCase`，常量保持 `UPPER_SNAKE_CASE`，公共 API 写 Doxygen 注释并置于对应模块子目录 (`core`、`driver`、`samples/*`) 中，命名空间优先 `namespace fclmusa::...`。PowerShell 函数为 PascalCase，变量 camelCase，并使用简洁块注释说明环境或副作用。

## 测试准则
为新增功能就近添加单元或集成测试，例如 `kernel/tests/DriverCollisionTests.cpp`、`tests/UserBroadphaseTests.cpp`。测试类命名 `*Tests`，关注可观察行为。快速冒烟可执行 `pwsh tools/build/test-tasks.ps1 -Task R3-Demo`，提交前务必跑通 `pwsh tools/scripts/run_all_tests.ps1`。需要 WDK 或硬件依赖时在测试文件头注明，并通过 `FCLMUSA_BUILD_DRIVER` 选项进行条件编译。

## Commit 与 Pull Request
遵循 Conventional Commits（如 `feat(kernel): add capsule support`），标题控制在 72 字符内，脚注写上 `Closes #123` 等关联 Issue。PR 描述应包含：变更背景、受影响的构建目标、验证命令/日志或截图，以及文档更新情况；若演示程序有 UI，附带运行截图。仅在 `build.ps1` -> Test 通过后请求评审，并确保格式化、静态检查与 API 文档同步完成。
