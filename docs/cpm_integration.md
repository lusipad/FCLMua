# CPM 集成指南（仅构建 FclMusa Core）

目标：让外部仓库通过 CPM 拉取本仓库源码，并用自身的 LLVM-MSVC + WDK 环境编译 `FclMusa::Core`（R0）或 `FclMusa::CoreUser`（R3）静态库（默认不构建驱动 `.sys`）。

## 先决条件
- Windows；若需要 R0，则需安装 WDK（km 头文件）。
- CMake ≥ 3.24，支持 CPM。
- 编译器：MSVC 或 clang-cl（推荐使用 VS 生成器并指定 `-T ClangCL` 以继承 WDK 环境）。

## CPM 用法示例
```cmake
include(cmake/CPM.cmake)

CPMAddPackage(
  NAME FclMusa
  GITHUB_REPOSITORY yourname/FCLMua   # 或 fork 的地址
  GIT_TAG v0.1.0                      # 固定版本
  OPTIONS
    "FCLMUSA_BUILD_DRIVER OFF"        # 不构建 .sys
    "FCLMUSA_BUILD_USERLIB ON"        # 需要 R3 版则打开
    "FCLMUSA_BUILD_KERNEL_LIB OFF"    # 无 WDK 的纯 R3 环境可关闭
    "FCLMUSA_WDK_ROOT C:/Program Files (x86)/Windows Kits/10"  # R0 需要
    "FCLMUSA_WDK_VERSION 10.0.22621.0"                         # R0 需要
)

# R0（内核）静态库
target_link_libraries(my_km_target PRIVATE FclMusa::Core)
# R3（用户态）静态库
target_link_libraries(my_um_target PRIVATE FclMusa::CoreUser)
```

## 关键变量
- `FCLMUSA_WDK_ROOT`：WDK 根目录，默认读取环境变量 `WDKContentRoot`（若存在）。仅 R0 需要。
- `FCLMUSA_WDK_VERSION`：WDK Include 目录中的版本号（如 `10.0.22621.0`）。如果使用 VS 生成器，默认会尝试使用 `CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION`。仅 R0 需要。
- `FCLMUSA_BUILD_DRIVER`：是否构建 `.sys` 驱动目标。默认 `OFF`，当前只声明占位，你可按自己环境扩展。
- `FCLMUSA_BUILD_USERLIB`：是否构建用户态静态库 `FclMusa::CoreUser`。默认 `ON`。
- `FCLMUSA_BUILD_KERNEL_LIB`：是否构建内核态静态库 `FclMusa::Core`。若检测不到 `FCLMUSA_WDK_ROOT`，默认关闭；若开启但未提供 WDK 路径/版本会直接报错。

## 编译提示
- Core 目标已设置 MSVC/clang-cl 兼容的 `/bigobj /utf-8 /W4 /FS`，并启用了必须的宏（`FCL_MUSA_KERNEL_MODE`、`FCL_STATIC_DEFINE`、`CCD_STATIC_DEFINE` 等）。
- 若未提供 WDK 变量，会自动只构建 R3（用户态）静态库；需要 R0 时请显式设置 WDK 路径/版本或打开 `FCLMUSA_BUILD_KERNEL_LIB`。
- WDK 头文件通过上述变量注入；如果你的工具链已经自动带入 km 头，可不显式设置。
- FCL 源码、Eigen、libccd 均使用仓库内置副本，无需外部下载。

## 产物
- 生成的静态库：`FclMusaCore.lib`
- 头文件包含目录：`kernel/core/include`（通过 `target_include_directories` 已导出，直接 `#include <fclmusa/...>` 即可）

## 已知限制
- 驱动 `.sys` 构建未在 CMake 中启用；如果需要，请在 `CMakeLists.txt` 的占位段自行按 WDK/LLVM-MSVC 环境添加。
- 如果你的项目对警告级别/异常模型有特殊要求，请在上层调整（目前默认 `/EHsc`，未开启警告即错）。 
