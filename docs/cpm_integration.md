# CPM 集成指南（仅构建 FclMusa Core）

目标：让外部仓库通过 CPM 拉取本仓库源码，并用自身的 LLVM-MSVC + WDK 环境编译 `FclMusa::Core`（内核态）或 `FclMusa::CoreUser`（用户态）静态库（默认不构建驱动 `.sys`）。

## 先决条件
- Windows；若需要内核态库，则需安装 WDK（km 头文件）。
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
    "FCLMUSA_BUILD_USERLIB ON"        # 需要用户态版则打开
    "FCLMUSA_BUILD_KERNEL_LIB OFF"    # 无 WDK 的纯用户态环境可关闭
    "FCLMUSA_WDK_ROOT C:/Program Files (x86)/Windows Kits/10"  # 内核态需要
    "FCLMUSA_WDK_VERSION 10.0.22621.0"                         # 内核态需要
  POST_DOWNLOAD_COMMAND               # 自动套用 external/fcl-source 补丁
    "${CMAKE_COMMAND}" -E chdir <SOURCE_DIR>/tools/scripts pwsh apply_fcl_patch.ps1
)

# 内核态静态库
target_link_libraries(my_km_target PRIVATE FclMusa::Core)
# 用户态静态库
target_link_libraries(my_um_target PRIVATE FclMusa::CoreUser)
```

## 开箱即用模板

仓库提供了一个可直接复用的模板：[`cmake/FclMusaCPMTemplate.cmake`](../cmake/FclMusaCPMTemplate.cmake)。

使用步骤：

1. 将该文件拷贝到你的工程（例如 `cmake/FclMusaCPMTemplate.cmake`）。
2. 在顶层 `CMakeLists.txt` 中，根据需要设置模板变量（下表）；
3. `include(cmake/FclMusaCPMTemplate.cmake)`；
4. 像平常一样 `target_link_libraries(my_target PRIVATE FclMusa::Core)` 或 `FclMusa::CoreUser`。

模板会自动：

- 如果 `FCLMUSA_CPM_ENABLE_R0=ON`，运行 `tools/scripts/setup_dependencies.ps1` 恢复 Musa.Runtime；
- 每次拉取时执行 `apply_fcl_patch.ps1`，确保内核补丁到位；
- 通过 CPM 暴露 `FclMusa::Core` / `FclMusa::CoreUser` 目标。

> ⚠️ 该模板需要可用的 PowerShell 7 (`pwsh`). 若在 CI 中运行，请提前安装 PowerShell 并将其加入 PATH。

### 模板变量速查

| 变量 | 默认值 | 说明 |
| --- | --- | --- |
| `FCLMUSA_CPM_VERSION` | `v0.1.0` | 指定要拉取的 tag/commit，生产环境建议固定具体版本 |
| `FCLMUSA_CPM_ENABLE_R3` | `ON` | 是否构建用户态静态库 `FclMusa::CoreUser` |
| `FCLMUSA_CPM_ENABLE_R0` | `OFF` | 是否构建内核态静态库 `FclMusa::Core`（需要 WDK + Musa.Runtime） |
| `FCLMUSA_CPM_ENABLE_DRIVER` | `OFF` | 预留给 `.sys` 构建（目前仍通过 msbuild，模板里保持关闭） |
| `FCLMUSA_CPM_WDK_ROOT` | `C:/Program Files (x86)/Windows Kits/10` | 仅在 `ENABLE_R0=ON` 时使用；可指向自定义 WDK 安装目录 |
| `FCLMUSA_CPM_WDK_VERSION` | `10.0.22621.0` | 仅在 `ENABLE_R0=ON` 时使用；指定 `Include/<version>/km` 子目录名 |

> **提示**：若你的工程目录已经存在 FCL+Musa 仓库，可以在顶层 CMakeLists.txt 中设置 `set(FclMusa_SOURCE "D:/Repos/FCL+Musa")`（或对应的绝对路径），模板会直接复用该源码目录，避免 CPM 重新克隆并触发 git stash 冲突。
>
> **WDK 版本**：请确认 `FCLMUSA_CPM_WDK_VERSION` 与本机 `C:/PROGRA~2/Windows Kits/10/Include/<version>/km` 中实际存在的版本一致，可用 `dir "C:/PROGRA~2/Windows Kits/10/Include"` 检查；版本不匹配时会出现 `ntddk.h` 找不到。

变量全部是 Cache 变量，可在命令行（`-DVAR=value`）或 `cmake-gui` 中覆盖。

### 场景示例

#### 1. 仅用户态（R3）SDK

```cmake
# CMakeLists.txt (节选)
set(FCLMUSA_CPM_VERSION "v0.1.0")
set(FCLMUSA_CPM_ENABLE_R3 ON)
set(FCLMUSA_CPM_ENABLE_R0 OFF)

include(cmake/FclMusaCPMTemplate.cmake)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE FclMusa::CoreUser)
```

适用：桌面/服务程序只需要用户态碰撞检测，无 WDK 依赖。

#### 2. 仅内核态（R0）静态库/驱动

```cmake
set(FCLMUSA_CPM_ENABLE_R3 OFF)
set(FCLMUSA_CPM_ENABLE_R0 ON)
set(FCLMUSA_CPM_WDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
set(FCLMUSA_CPM_WDK_VERSION "10.0.22621.0")

include(cmake/FclMusaCPMTemplate.cmake)

add_library(MyDriverCore STATIC driver_core.cpp)
target_link_libraries(MyDriverCore PRIVATE FclMusa::Core)
```

模板会自动恢复 Musa.Runtime，MyDriverCore 便能在 R0 中使用 STL/异常并直接调用 Fcl API。

#### 3. 双模（R0 + R3）同时构建

```cmake
set(FCLMUSA_CPM_ENABLE_R3 ON)
set(FCLMUSA_CPM_ENABLE_R0 ON)

include(cmake/FclMusaCPMTemplate.cmake)

add_executable(my_gui samples/gui_app.cpp)
target_link_libraries(my_gui PRIVATE FclMusa::CoreUser)

add_library(my_kmdf STATIC kmdf_entry.cpp)
target_link_libraries(my_kmdf PRIVATE FclMusa::Core)
```

适用：需要在同一工程里编译用户态工具和配套内核模块，保证两侧使用同一版本。

> **建议**：把自定义算法/数据结构放在一个公共静态库（如 `add_library(CollisionCommon ...)`）中，源码里统一 `#include <fclmusa/platform.h>` 并使用 `FCL_MUSA_KERNEL_MODE` 做最小的条件编译。该库在 R3 构建时链接 `FclMusa::CoreUser`，R0 构建时链接 `FclMusa::Core`，从而在两侧都可以使用 `std::vector`、`std::unique_ptr` 等标准库类型——内核态由模板自动恢复的 Musa.Runtime 提供实现，用户态则走 MSVC CRT。入口层再各自处理 IOCTL/IRQL 差异即可。

#### 4. 仅拉取核心头文件以做接口扫描/定制构建

```cmake
set(FCLMUSA_CPM_ENABLE_R3 OFF)
set(FCLMUSA_CPM_ENABLE_R0 OFF)
set(FCLMUSA_CPM_ENABLE_DRIVER OFF)

include(cmake/FclMusaCPMTemplate.cmake)

# 仅使用头文件（例如做代码生成或接口校验）
target_include_directories(my_tool PRIVATE "${FclMusa_SOURCE_DIR}/kernel/core/include")
```

适用：CI 中只想借用头文件或做静态分析，不需要构建任何库。

## 关键变量
- `FCLMUSA_WDK_ROOT`：WDK 根目录，默认读取环境变量 `WDKContentRoot`（若存在）。仅内核态库需要。
- `FCLMUSA_WDK_VERSION`：WDK Include 目录中的版本号（如 `10.0.22621.0`）。如果使用 VS 生成器，默认会尝试使用 `CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION`。仅内核态库需要。
- `FCLMUSA_BUILD_DRIVER`：是否构建 `.sys` 驱动目标。默认 `OFF`，当前只声明占位，你可按自己环境扩展。
- `FCLMUSA_BUILD_USERLIB`：是否构建用户态静态库 `FclMusa::CoreUser`。默认 `ON`。
- `FCLMUSA_BUILD_KERNEL_LIB`：是否构建内核态静态库 `FclMusa::Core`。若检测不到 `FCLMUSA_WDK_ROOT`，默认关闭；若开启但未提供 WDK 路径/版本会直接报错。

## 编译提示
- Core 目标已设置 MSVC/clang-cl 兼容的 `/bigobj /utf-8 /W4 /FS`，并启用了必须的宏（`FCL_MUSA_KERNEL_MODE`、`FCL_STATIC_DEFINE`、`CCD_STATIC_DEFINE` 等）。
- 若未提供 WDK 变量，会自动只构建用户态静态库；需要内核态库时请显式设置 WDK 路径/版本或打开 `FCLMUSA_BUILD_KERNEL_LIB`。
- WDK 头文件通过上述变量注入；如果你的工具链已经自动带入 km 头，可不显式设置。
- FCL 源码、Eigen、libccd 均使用仓库内置副本，无需外部下载。

## 产物
- 生成的静态库：`FclMusaCore.lib`
- 头文件包含目录：`kernel/core/include`（通过 `target_include_directories` 已导出，直接 `#include <fclmusa/...>` 即可）

## 已知限制
- 驱动 `.sys` 构建未在 CMake 中启用；如果需要，请在 `CMakeLists.txt` 的占位段自行按 WDK/LLVM-MSVC 环境添加。
- 如果你的项目对警告级别/异常模型有特殊要求，请在上层调整（目前默认 `/EHsc`，未开启警告即错）。 
- `external/fcl-source` 默认跟随上游 FCL v0.7.0。若直接通过 CPM 拉取，需要在下载完成后执行 `tools/scripts/apply_fcl_patch.ps1`（推荐通过 `POST_DOWNLOAD_COMMAND` 自动完成），或使用已预打补丁的 fork 仓库，否则内核模式补丁不会应用。
