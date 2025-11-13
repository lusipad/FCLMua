# FCL+Musa 项目文件层次结构文档

本文档详细介绍 FCL+Musa 项目的完整文件层次结构，每个目录和文件的功能与作用。

---

## 目录

- [项目根目录](#项目根目录)
- [kernel/ - 内核驱动](#kernel---内核驱动)
  - [driver/src/ - 驱动源代码](#driversrc---驱动源代码)
  - [driver/include/ - 驱动头文件](#driverinclude---驱动头文件)
- [fcl-source/ - FCL 上游源码](#fcl-source---fcl-上游源码)
- [tools/ - 工具集](#tools---工具集)
- [docs/ - 文档](#docs---文档)
- [external/ - 外部依赖](#external---外部依赖)
- [openspec/ - 变更规范](#openspec---变更规范)

---

## 项目根目录

```
FCL+Musa/
├── README.md                  # 项目主要说明文档
├── CLAUDE.md                  # Claude Code AI 助手配置
├── AGENTS.md                  # AI 代理配置文档
├── build_driver.cmd           # Windows 驱动构建脚本（主入口）
├── .gitignore                 # Git 忽略规则
├── kernel/                    # 内核驱动模块
├── fcl-source/                # FCL 上游库源码
├── tools/                     # 开发工具与测试程序
├── docs/                      # 项目文档
├── external/                  # 外部依赖库
└── openspec/                  # OpenSpec 变更管理
```

### 根目录文件说明

| 文件名 | 作用 |
|--------|------|
| **README.md** | 项目概览、安装说明、IOCTL 接口、使用示例 |
| **build_driver.cmd** | 主构建脚本，自动初始化 VsDevCmd、WDK 环境并编译驱动 |
| **CLAUDE.md** | Claude Code 项目指令，包含 OpenSpec 配置 |
| **AGENTS.md** | AI 代理系统配置（任务分解、代码审查等） |

---

## kernel/ - 内核驱动

内核驱动的核心代码，实现 Windows Ring 0 层的 FCL 碰撞检测功能。

```
kernel/
├── FclMusaDriver/             # Visual Studio 驱动工程
│   ├── FclMusaDriver.vcxproj  # VS 项目文件
│   └── FclMusaDriver.vcxproj.filters  # VS 文件过滤器
└── driver/                    # 驱动实际代码目录
    ├── include/               # 公共头文件
    └── src/                   # 源代码实现
```

### driver/src/ - 驱动源代码

驱动的所有实现代码，按功能模块组织。

#### 核心模块

```
src/
├── driver_entry.cpp           # 驱动入口点（DriverEntry、Unload）
├── driver_state.cpp           # 驱动全局状态管理
├── device_control.cpp         # IOCTL 分发处理器
```

**文件说明：**

| 文件 | 功能 | 关键函数 |
|------|------|---------|
| **driver_entry.cpp** | 驱动生命周期管理 | `DriverEntry`, `DriverUnload` |
| **driver_state.cpp** | 初始化各子系统（内存、几何、碰撞） | `FclInitialize`, `FclCleanup` |
| **device_control.cpp** | IOCTL 命令路由与处理 | `FclDeviceControl` (处理 0x800-0x808 的 IOCTL) |

#### 几何管理 (geometry/)

```
src/geometry/
├── geometry_manager.cpp       # 几何对象生命周期管理
├── bvh_model.cpp              # BVH 层次结构构建与管理
└── obbrss.cpp                 # OBBRSS 包围盒计算
```

**功能说明：**
- **geometry_manager.cpp**: 管理 Sphere/OBB/Mesh 几何句柄、AVL 树索引、引用计数
- **bvh_model.cpp**: 为 Mesh 构建 OBBRSS 层次结构（用于加速碰撞检测）
- **obbrss.cpp**: 实现 OBB + RSS 混合包围盒的计算逻辑

#### 碰撞检测 (collision/)

```
src/collision/
├── collision.cpp              # 离散碰撞检测入口
├── continuous_collision.cpp   # 连续碰撞检测（CCD）
└── collision_self_test.cpp    # 碰撞模块自测代码
```

**功能说明：**
- **collision.cpp**: 实现 `FclCollisionDetect`, `FclCollideObjects`
  - 调用 upstream FCL 的 `fcl::collide()`
  - 支持 Sphere/OBB/Mesh 任意组合
- **continuous_collision.cpp**: 实现 `FclContinuousCollision`
  - 支持 InterpMotion（线性插值运动）
  - 支持 ScrewMotion（螺旋运动）
  - 调用 `fcl::continuousCollide()` (Conservative Advancement 算法)
- **collision_self_test.cpp**: 碰撞模块的单元测试与回归测试

#### 距离计算 (distance/)

```
src/distance/
└── distance.cpp               # 几何对象间距离计算
```

**功能说明：**
- 实现 `FclDistanceCompute` API
- 返回最短距离及最近点对
- 调用 upstream FCL 的 `fcl::distance()`

#### 宽相检测 (broadphase/)

```
src/broadphase/
└── broadphase.cpp             # 宽相碰撞对筛选
```

**功能说明：**
- 实现 `FclBroadphaseDetect` API
- 使用 FCL 的 `DynamicAABBTreeCollisionManager` 进行快速粗检测
- 输出可能发生碰撞的几何对

#### Upstream 桥接层 (upstream/)

```
src/upstream/
├── upstream_bridge.cpp        # FCL 算法调用封装
└── geometry_bridge.cpp        # 几何对象适配转换
```

**关键作用：**
- **upstream_bridge.cpp**:
  - `FclUpstreamCollide()` - 调用 `fcl::collide()`
  - `FclUpstreamDistance()` - 调用 `fcl::distance()`
  - `FclUpstreamContinuousCollision()` - 调用 `fcl::continuousCollide()`
  - 异常处理：try/catch 转换为 NTSTATUS
- **geometry_bridge.cpp**:
  - `BuildGeometryBinding()` - 将驱动几何转为 FCL `CollisionGeometry`
  - `BuildMeshGeometry()` - 构建 `BVHModel<OBBRSS>`
  - `BuildCollisionObjects()` - 创建 FCL 碰撞对象

#### 内存管理 (memory/)

```
src/memory/
└── pool_allocator.cpp         # NonPagedPool 分配器
```

**功能说明：**
- 实现内核兼容的 STL 分配器
- 统计内存使用情况（池大小、分配次数、峰值）
- 供 Eigen、libccd、FCL 使用

#### 运行时适配 (runtime/)

```
src/runtime/
├── musa_runtime_adapter.cpp   # Musa.Runtime 初始化
└── c_runtime_stubs.cpp        # C 运行时桩函数
```

**功能说明：**
- **musa_runtime_adapter.cpp**: 初始化 Musa.Runtime（提供 STL/异常支持）
- **c_runtime_stubs.cpp**: 内核不支持的 C 运行时函数桩实现

#### 数学支持 (math/)

```
src/math/
└── eigen_self_test.cpp        # Eigen 库自测
```

**功能说明：**
- 验证 Eigen 矩阵运算在内核环境下的正确性
- 测试向量、矩阵、四元数操作

#### Narrowphase 支持 (narrowphase/)

```
src/narrowphase/
└── libccd_memory.cpp          # libccd 内存钩子
```

**功能说明：**
- 为 libccd（GJK/EPA 算法库）提供内核兼容的内存分配
- 将 `ccdRealloc/ccdFree` 映射到 NonPagedPool

#### 测试模块 (testing/)

```
src/testing/
└── self_test.cpp              # 驱动自测总入口
```

**功能说明：**
- 实现 `FclRunSelfTest()` API
- 执行全部自检场景：
  - 初始化测试
  - 几何创建/销毁
  - 碰撞检测（Sphere/OBB/Mesh）
  - 连续碰撞（CCD）
  - 压力测试（128 次 Mesh↔Mesh）
  - Driver Verifier 兼容性
- 输出 `FCL_SELF_TEST_RESULT` 结构

---

### driver/include/ - 驱动头文件

所有公共 API 和内部接口定义。

```
include/
└── fclmusa/                   # 主命名空间
    ├── version.h              # 版本号定义
    ├── driver.h               # 驱动主头文件
    ├── ioctl.h                # IOCTL 代码和结构体定义
    ├── logging.h              # 内核日志宏
    ├── geometry.h             # 几何对象 API
    ├── collision.h            # 碰撞检测 API
    ├── distance.h             # 距离计算 API
    ├── broadphase.h           # 宽相检测 API
    ├── self_test.h            # 自测 API
    ├── geometry/              # 几何子模块
    │   ├── bvh_model.h        # BVH 构建接口
    │   ├── obbrss.h           # OBBRSS 包围盒定义
    │   ├── obb.h              # OBB 定义
    │   └── math_utils.h       # 数学工具函数
    ├── memory/
    │   └── pool_allocator.h   # 内存分配器接口
    ├── runtime/
    │   └── musa_runtime_adapter.h  # Musa.Runtime 接口
    ├── upstream/              # FCL 桥接层
    │   ├── upstream_bridge.h  # 算法调用接口
    │   └── geometry_bridge.h  # 几何转换接口
    ├── math/
    │   ├── eigen_config.h     # Eigen 配置（禁用对齐）
    │   └── self_test.h        # 数学自测接口
    ├── narrowphase/
    │   └── libccd_memory.h    # libccd 内存钩子
    └── collision/
        └── self_test.h        # 碰撞自测接口
```

#### 核心头文件说明

| 头文件 | 功能 | 关键类型/函数 |
|--------|------|--------------|
| **geometry.h** | 几何对象 API | `FCL_GEOMETRY_HANDLE`, `FCL_MESH_GEOMETRY_DESC`, `FclCreateGeometry()` |
| **collision.h** | 碰撞检测 API | `FCL_CONTACT_INFO`, `FclCollisionDetect()`, `FclCollideObjects()` |
| **distance.h** | 距离计算 API | `FCL_DISTANCE_RESULT`, `FclDistanceCompute()` |
| **broadphase.h** | 宽相检测 API | `FclBroadphaseDetect()` |
| **ioctl.h** | IOCTL 定义 | `IOCTL_FCL_PING` (0x800), `IOCTL_FCL_CREATE_MESH` (0x808) 等 |
| **self_test.h** | 自测 API | `FCL_SELF_TEST_RESULT`, `FclRunSelfTest()` |

---

## fcl-source/ - FCL 上游源码

嵌入的 upstream FCL 库源代码（commit: 5f7776e2101b8ec95d5054d732684d00dac45e3d）。

```
fcl-source/
├── include/fcl/               # FCL 公共头文件
│   ├── broadphase/            # 宽相检测（动态 AABB 树等）
│   ├── narrowphase/           # 窄相检测（collide, distance）
│   ├── geometry/              # 几何形状（Sphere, Box, BVHModel）
│   │   ├── shape/             # 基础形状
│   │   └── bvh/               # BVH 模型
│   ├── math/                  # 数学库（Transform, BV 等）
│   └── common/                # 通用工具（类型、Profiler）
├── src/                       # FCL 实现代码
│   ├── broadphase/
│   ├── narrowphase/
│   ├── geometry/
│   ├── math/
│   └── common/
├── CMakeLists.txt             # CMake 构建配置（未使用）
└── README.md                  # FCL 原版 README
```

**说明：**
- 该目录是 FCL 原版库的完整拷贝
- 驱动通过 `#include <fcl/...>` 直接使用这些头文件
- 不修改 FCL 源码，仅通过内存/日志钩子适配到内核环境
- 用于保持与上游一致，便于升级

---

## tools/ - 工具集

开发、构建、测试、部署工具。

```
tools/
├── fcl_demo.cpp               # 用户态 CLI 测试程序（Ring 3）
├── build_demo.cmd             # 编译 fcl_demo.exe
├── manual_build.cmd           # 驱动手动构建脚本
├── sign_driver.ps1            # 驱动签名脚本（测试证书）
├── fcl-self-test.ps1          # 驱动自测脚本
├── verify_upstream.ps1        # 验证驱动输出与 FCL 一致性
├── setup-hyperv-lab.ps1       # Hyper-V 测试环境搭建
├── assets/                    # 测试资源
│   └── cube.obj               # 立方体模型（8 顶点，12 三角形）
├── scenes/                    # 测试场景脚本
│   ├── two_spheres.txt        # 双球碰撞场景
│   ├── mesh_probe.txt         # Mesh + Sphere 探测
│   └── arena_mix.txt          # 复杂混合场景
└── build/                     # 编译输出目录（生成的可执行文件）
```

### 工具文件详解

#### fcl_demo.cpp
**功能**: 用户态 CLI 交互程序
- **命令**:
  - `load <name> <obj_path>` - 加载 OBJ 模型
  - `sphere <name> <radius> [x y z]` - 创建球体
  - `move <name> <dx dy dz>` - 移动对象
  - `collide <obj1> <obj2>` - 碰撞检测
  - `distance <obj1> <obj2>` - 距离计算
  - `ccd <obj1> <obj2> <dx dy dz>` - 连续碰撞
  - `simulate <obj1> <obj2> <dx dy dz> <steps> <ms>` - 运动仿真
  - `run <scene_file>` - 批量执行场景文件
- **IOCTL 通信**: 通过 `DeviceIoControl` 与驱动交互

#### fcl-self-test.ps1
**功能**: 自动化测试脚本
- 发送 `IOCTL_FCL_PING` 查询驱动状态
- 发送 `IOCTL_FCL_SELF_TEST` 执行全部自测
- 解析 `FCL_SELF_TEST_RESULT` 并输出报告
- 返回退出码：0 = 成功，10 = 失败

#### verify_upstream.ps1
**功能**: 验证驱动与 upstream FCL 的一致性
- 执行碰撞/距离/CCD 测试场景
- 调用驱动 IOCTL 获取结果
- 与预期值（upstream FCL 计算）比对
- 支持 `-Tolerance` 浮点误差容忍
- 支持 `-Json` 输出格式（适合 CI）

#### sign_driver.ps1
**功能**: 自动签名驱动
- 检测是否存在 `CN=FclMusaTestCert` 证书
- 不存在则自动生成自签名证书
- 使用 `signtool` 签名 `FclMusaDriver.sys`
- 输出 `.cer` 和 `.pfx` 到输出目录

#### manual_build.cmd
**功能**: 驱动构建脚本
- 初始化 Visual Studio 开发环境（VsDevCmd）
- 设置 WDK 路径
- 调用 `msbuild` 清理并编译驱动
- 自动调用 `sign_driver.ps1` 进行签名

---

## docs/ - 文档

项目的完整技术文档。

```
docs/
├── api.md                     # API 参考手册
├── architecture.md            # 架构设计说明
├── usage.md                   # 使用指南
├── demo.md                    # Ring 0/Ring 3 示例代码
├── testing.md                 # 测试策略与覆盖范围
├── deployment.md              # 部署流程（安装/卸载/签名）
├── release_notes.md           # 版本发布说明
├── known_issues.md            # 已知问题与限制
├── VM_DEBUG_SETUP.md          # 虚拟机调试环境搭建
└── EIGEN_ADAPTATION.md        # Eigen 库内核适配说明
```

### 文档内容概览

| 文档 | 内容 |
|------|------|
| **api.md** | 所有 API 函数签名、参数、返回值详解 |
| **architecture.md** | 模块划分、数据流、依赖关系、Upstream 集成 |
| **usage.md** | 快速上手、典型场景、IOCTL 使用示例 |
| **demo.md** | Ring 0 驱动内示例、Ring 3 用户态示例 |
| **testing.md** | 自测覆盖、压力测试、Verifier 验证 |
| **deployment.md** | 驱动安装、证书导入、启动/停止命令 |
| **release_notes.md** | 版本更新历史、新增功能、修复问题 |
| **known_issues.md** | 当前已知 Bug、待办事项、限制说明 |
| **VM_DEBUG_SETUP.md** | WinDbg 网络调试、符号路径、断点技巧 |
| **EIGEN_ADAPTATION.md** | Eigen 库在内核环境的适配细节 |

---

## external/ - 外部依赖

第三方库与运行时支持。

```
external/
└── Musa.Runtime/              # 内核 STL/异常支持库
    ├── Musa.Runtime/          # 核心运行时
    │   ├── Musa.Runtime.props  # VS 属性表
    │   └── MSVC/              # 不同 MSVC 版本的 CRT 适配
    ├── Publish/               # 发布的 NuGet 包配置
    └── Microsoft.VisualC.Runtime/  # VC++ 运行时头文件
```

**说明：**
- **Musa.Runtime**: 提供内核兼容的 C++ 标准库（std::vector, std::shared_ptr, 异常等）
- 驱动依赖此库才能使用 C++ 特性（FCL 需要 STL）
- 通过 `.props` 文件集成到 Visual Studio 项目

---

## openspec/ - 变更规范

OpenSpec 项目管理系统，用于跟踪架构变更和特性开发。

```
openspec/
├── AGENTS.md                  # OpenSpec 代理指令
├── project.md                 # 项目元信息
├── specs/                     # 能力规范定义
│   ├── collision-detection.md  # 碰撞检测能力
│   ├── memory-management.md    # 内存管理能力
│   └── ...
└── changes/                   # 变更记录
    └── update-use-upstream-fcl/  # 迁移到 upstream FCL 的变更
        ├── design.md          # 设计方案
        ├── tasks.md           # 任务清单
        └── implementation.md  # 实施记录
```

**说明：**
- **specs/**: 定义项目的核心能力（collision-detection, memory-management 等）
- **changes/**: 每个重大变更有独立目录，包含设计、任务、实施文档
- **AGENTS.md**: 指导 AI 助手如何创建和应用变更提案

---

## 项目文件关系图

```
用户空间 (Ring 3)
┌─────────────────────────────┐
│ tools/fcl_demo.exe          │ ← 编译自 fcl_demo.cpp
│   ├─ 加载 OBJ 模型           │
│   ├─ 发送 IOCTL              │
│   └─ 解析结果                │
└──────────┬──────────────────┘
           │ DeviceIoControl (IOCTL)
           ↓
内核空间 (Ring 0)
┌─────────────────────────────┐
│ FclMusaDriver.sys           │ ← 编译自 kernel/driver/src/
│ ┌─────────────────────────┐ │
│ │ device_control.cpp      │ │ ← IOCTL 分发
│ └──────┬──────────────────┘ │
│        ↓                    │
│ ┌─────────────────────────┐ │
│ │ collision.cpp           │ │ ← 碰撞检测入口
│ │   ↓                     │ │
│ │ upstream_bridge.cpp     │ │ ← 调用 FCL
│ │   ↓                     │ │
│ │ geometry_bridge.cpp     │ │ ← 几何转换
│ └──────┬──────────────────┘ │
│        ↓                    │
│ ┌─────────────────────────┐ │
│ │ fcl-source/             │ │ ← FCL 原版算法
│ │   fcl::collide()        │ │
│ │   fcl::distance()       │ │
│ │   fcl::continuousCollide│ │
│ └─────────────────────────┘ │
└─────────────────────────────┘
```

---

## 构建流程

```
1. build_driver.cmd
   ↓
2. 初始化 VsDevCmd (Visual Studio 环境)
   ↓
3. msbuild kernel/FclMusaDriver/FclMusaDriver.vcxproj
   ├─ 编译 driver/src/*.cpp
   ├─ 链接 Musa.Runtime
   ├─ 链接 fcl-source (头文件引用)
   └─ 生成 FclMusaDriver.sys
   ↓
4. sign_driver.ps1
   ├─ 生成测试证书 (CN=FclMusaTestCert)
   ├─ signtool sign FclMusaDriver.sys
   └─ 输出 .cer 和 .pfx
```

---

## 关键文件快速索引

### 驱动开发
- **驱动入口**: `kernel/driver/src/driver_entry.cpp`
- **IOCTL 处理**: `kernel/driver/src/device_control.cpp`
- **碰撞检测**: `kernel/driver/src/collision/collision.cpp`
- **FCL 桥接**: `kernel/driver/src/upstream/upstream_bridge.cpp`

### API 定义
- **几何 API**: `kernel/core/include/fclmusa/geometry.h`
- **碰撞 API**: `kernel/core/include/fclmusa/collision.h`
- **IOCTL 定义**: `kernel/core/include/fclmusa/ioctl.h`

### 测试与工具
- **用户态测试**: `tools/fcl_demo.cpp`
- **驱动自测**: `kernel/driver/src/testing/self_test.cpp`
- **验证脚本**: `tools/verify_upstream.ps1`

### 文档
- **API 参考**: `docs/api.md`
- **架构说明**: `docs/architecture.md`
- **使用指南**: `docs/usage.md`

---

## 文件命名约定

### 源代码
- `.cpp` - C++ 实现文件
- `.h` - C/C++ 头文件
- `-inl.h` - 内联函数实现（FCL 约定）

### 脚本
- `.cmd` - Windows 批处理脚本
- `.ps1` - PowerShell 脚本

### 文档
- `.md` - Markdown 文档
- 全大写（如 `README.md`, `CLAUDE.md`）- 项目级别文档
- 小写（如 `api.md`, `usage.md`）- 技术文档

---

## 总结

FCL+Musa 项目采用清晰的分层架构：

1. **kernel/driver** - 内核驱动核心，负责 Windows 环境适配
2. **fcl-source** - FCL 上游库，提供碰撞检测算法
3. **tools** - 开发工具与测试程序
4. **docs** - 完整技术文档
5. **external** - 外部依赖（Musa.Runtime）
6. **openspec** - 变更管理系统

每个模块职责明确，通过 `upstream_bridge` 实现内核与 FCL 的无缝集成，确保算法与原版 FCL 100% 一致。

