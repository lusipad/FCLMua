# FCL+Musa 项目文件层次结构文档

本文档详细介绍 FCL+Musa 项目的完整文件层次结构，每个目录和文件的功能与作用。

---

## 目录

- [项目根目录](#项目根目录)
- [kernel/ - 内核驱动](#kernel---内核驱动)
  - [core/ - FCL 核心模块](#core---fcl-核心模块)
  - [driver/ - 驱动封装层](#driver---驱动封装层)
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
├── QUICKSTART.md              # 快速开始指南
├── Directory.Build.targets    # MSBuild 全局构建目标（强制链接 Release 库）
├── .gitignore                 # Git 忽略规则
├── .github/                   # GitHub Actions CI/CD 配置
├── dist/                      # 构建产物输出目录
├── kernel/                    # 内核驱动模块
├── fcl-source/                # FCL 上游库源码
├── tools/                     # 开发工具与测试程序
├── docs/                      # 项目文档
├── external/                  # 外部依赖库
├── openspec/                  # OpenSpec 变更管理
└── archive/                   # 归档的历史文件
```

### 根目录文件说明

| 文件名 | 作用 |
|--------|------|
| **README.md** | 项目概览、构建说明、使用指南 |
| **QUICKSTART.md** | 极简上手指南 |
| **Directory.Build.targets** | 关键构建配置，防止 Debug 模式下的 CRT 冲突 |

---

## kernel/ - 内核驱动

内核驱动的核心代码，分为核心逻辑层 (core)、自测模块 (selftest) 和驱动适配层 (driver)。

```
kernel/
├── FclMusaDriver/             # Visual Studio 驱动工程文件
├── core/                      # FCL 核心逻辑库
│   ├── include/               # 核心头文件
│   └── src/                   # 核心实现代码
├── selftest/                  # 自测与回归测试模块
└── driver/                    # 驱动入口与 WDF 适配
    └── src/                   # 驱动层实现
```

### core/src/ - FCL 核心模块

负责几何管理、碰撞算法桥接、内存管理等，不依赖特定驱动框架，纯 C/C++ 实现。

```
kernel/core/src/
├── driver_state.cpp           # 全局状态管理（初始化/清理）
├── geometry/                  # 几何对象管理 (geometry_manager.cpp, bvh_model.cpp)
├── collision/                 # 碰撞检测 (collision.cpp, continuous_collision.cpp)
├── distance/                  # 距离计算 (distance.cpp)
├── broadphase/                # 宽相检测 (broadphase.cpp)
├── memory/                    # 内存分配器 (pool_allocator.cpp)
└── upstream/                  # FCL 桥接层 (upstream_bridge.cpp)
```

### selftest/ - 自测模块

包含所有内核自检、回归测试和单元测试逻辑，通过 `IOCTL_FCL_SELF_TEST` 触发。

```
kernel/selftest/
├── include/                   # 自测头文件 (self_test.h)
└── src/                       # 自测实现
    ├── self_test.cpp          # 自测主流程
    ├── collision_self_test.cpp # 碰撞场景测试
    ├── eigen_self_test.cpp    # Eigen 数学库测试
    └── eigen_extended_test.cpp # Eigen 扩展测试
```

### driver/src/ - 驱动封装层

负责处理 DriverEntry、IRP 分发、IOCTL 解析。

```
kernel/driver/src/
├── driver_entry.cpp           # 驱动入口点 (DriverEntry, DriverUnload)
├── device_control.cpp         # IOCTL 分发与参数校验
└── device_control_demo.cpp    # Demo 专用 IOCTL 处理
```

---

## tools/ - 工具集

构建、测试、部署脚本。

```
tools/
├── build_all.ps1              # [核心] 一键构建驱动、CLI、GUI 并打包
├── manual_build.cmd           # 驱动构建脚本（被 build_all.ps1 调用）
├── build_demo.cmd             # CLI Demo 构建脚本
├── sign_driver.ps1            # 驱动签名脚本
├── manage_driver.ps1          # 驱动安装/卸载/启停管理
├── fcl-self-test.ps1          # 驱动自测脚本
├── verify_upstream.ps1        # 上游一致性验证脚本
├── fcl_demo.cpp               # CLI 演示程序源码
└── gui_demo/                  # GUI 演示程序目录
```

---

## docs/ - 文档

| 文档 | 内容 |
|------|------|
| **api.md** | 内核 C API 与 IOCTL 接口参考 |
| **architecture.md** | 架构设计、分层说明、关键流程 |
| **file_structure.md** | (本文档) 文件结构说明 |
| **usage.md** | 详细构建与使用指南 |
| **deployment.md** | 部署与签名指南 |
| **demo.md** | 代码示例 |
| **testing.md** | 测试策略与覆盖率 |
| **vm_debug_setup.md** | 调试环境配置 |

---

## openspec/ - 变更管理

遵循 OpenSpec 规范的项目演进记录。

```
openspec/
├── AGENTS.md                  # AI 协作规范
├── project.md                 # 项目上下文
├── specs/                     # 现有能力规范
└── changes/                   # 变更提案与记录
```
