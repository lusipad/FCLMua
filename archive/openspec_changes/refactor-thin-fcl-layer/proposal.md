# 重构 FCL 分层与超薄封装

## Why（为什么）

当前驱动工程中，FCL 相关代码存在以下问题：

- **封装层过厚**：`geometry_manager.cpp`、`bvh_model.cpp`、`collision.cpp`、`distance.cpp`、`continuous_collision.cpp` 等文件在驱动项目内实现了完整的几何对象管理、BVH 构建、碰撞/距离/CCD 控制流，相当于在 upstream FCL 之外又构建了一套新的“运行时模型”。
- **职责边界不清晰**：IOCTL 处理层、FCL 管理模块、upstream FCL 之间的职责混在一起，使得：
  - FCL 的“管理逻辑”和驱动的 IOCTL/IRP 逻辑交织；
  - 很难看清哪些行为是“FCL 的算法/策略”，哪些只是“驱动封装”。
- **与设计预期不符**：产品侧期望的是：
  - 一个 **特别薄的 FCL 包装层**；
  - 清晰的“FCL 管理模块”负责资源/状态；
  - IOCTL 只对管理模块做简单包装，而不自带业务/控制逻辑。

为满足上述预期，需要通过一次专门的重构，重新定义分层边界，瘦身 IOCTL 封装层，并将管理/控制逻辑明确归属到 FCL 管理模块，而算法只保留在 upstream FCL 中。

## What Changes（要做什么）

### 新的三层分层模型

1. **第 0 层：upstream FCL（fcl-source + libccd + Eigen）**
   - 保留全部几何/碰撞/距离/CCD 算法实现；
   - 通过极薄的 bridge（`upstream_bridge.cpp` 等）暴露给内核侧 C 接口；
   - 不感知 IOCTL/IRP/设备对象，只处理几何和数学问题。

2. **第 1 层：FCL 内核管理模块（FclCore/FclMusaCoreLib）**
   - 职责：
     - 几何对象生命周期：`FclCreateGeometry` / `FclDestroyGeometry` / `FclUpdateMeshGeometry` / `FclAcquireGeometryReference` / `FclReleaseGeometryReference`；
     - BVH 缓存与绑定（必要时只做管理，不重复实现 FCL 算法）；
     - 碰撞/距离/CCD 调用流程：`FclCollisionDetect` / `FclCollideObjects` / `FclDistanceCompute` / `FclContinuousCollision`；
     - 自测和健康检查：`FclRunSelfTest` / `FclQueryHealth`；
     - 内存池与统计：`FclPoolAllocator`、`FCL_POOL_STATS` 等。
   - 要求：
     - **不再实现新的几何/碰撞算法**，而是通过 bridge 调用 upstream FCL；
     - 不依赖 IRP/IOCTL 的概念，对上暴露纯 C + NTSTATUS API。

3. **第 2 层：驱动 / IOCTL 封装层（当前 FclMusaDriver 工程）**
   - 职责：
     - 处理 `IRP_MJ_CREATE/CLOSE/DEVICE_CONTROL`；
     - 校验输入输出缓冲区长度、对齐、对齐到 `ULONG` 等；
     - 将 IOCTL buffer 映射为 FCL 管理模块的结构体和函数调用；
     - 不包含几何管理、BVH 构建、调度策略等控制逻辑。

### 具体重构方向

1. **澄清与复用现有“管理逻辑”**
   - 将 `geometry_manager.cpp` / `bvh_model.cpp` / `collision.cpp` / `distance.cpp` / `continuous_collision.cpp` / `self_test.cpp` 等文件视作“FCL 管理模块”的一部分，而非驱动 IOCTL 逻辑；
   - 统一 API 入口到 `fclmusa/geometry.h`、`fclmusa/collision.h`、`fclmusa/distance.h`、`fclmusa/self_test.h` 等头文件；
   - 在设计上强调：这些 API 是“FCL 内核库”的 C 接口，对内可以调用 upstream FCL 算法，但不会反向依赖 IOCTL。

2. **瘦身 IOCTL 层**
   - 限制 IOCTL 层的职责为：IRP 处理 + 结构体编解码 + 参数校验：
     - `device_control.cpp` 中的 `HandlePing` / `HandleSelfTest` / `HandleCollisionQuery` / `HandleDistanceQuery` / `HandleCreateSphere` / `HandleDestroyGeometry` / `HandleSphereCollisionDemo` / `HandleCreateMesh` / `HandleConvexCcdDemo` 等函数，只负责：
       - 检查 `InputBufferLength` / `OutputBufferLength` 是否满足对应的 IOCTL 结构体；
       - 将 SystemBuffer 视为某个 FCL IO buffer 结构体，调用 FCL 管理模块 API；
       - 写回结果 + 填写 `irp->IoStatus.Information`；
     - 避免在 IOCTL 层引入新的几何/碰撞控制逻辑或状态管理。

3. **算法归位到 upstream FCL**
   - 对当前 `bvh_model.cpp` 中实现的 BVH 构建/更新逻辑做一次评估：
     - 能复用 upstream FCL 的 `BVHModel` 能力的地方，优先走 upstream；
     - 保留确有必要的轻量封装（例如为内核友好的数据结构做桥接），但不再重复实现完整 BVH 算法；
   - 确认碰撞/距离/CCD 控制流程中：
     - 核心计算都通过 `upstream_bridge.cpp` 调用 `fcl::collide` / `fcl::distance` / `fcl::continuousCollide`；
     - 内部不再新增独立于 upstream FCL 的几何/碰撞算法实现。

4. **接口与分层文档化**
   - 在 `docs/architecture.md` 和 `docs/api.md` 中补充：
     - 三层分层说明（upstream FCL / FCL 管理模块 / IOCTL 封装）；
     - 每层的输入输出边界与典型调用链；
     - 明确“FCL 薄封装”的设计原则：
       - IOCTL 层不做控制逻辑；
       - 管理层不重造算法轮子；
       - 算法层只负责几何和数学，不碰 IRP。

## Impact（影响）

- **架构清晰度**：
  - 对使用者来说，FCL 暴露的是一组稳定的 C 接口（管理模块），驱动只是一个“传话筒”；
  - 便于在不改 IOCTL 协议的前提下，替换或优化内部管理/算法实现。

- **维护成本**：
  - FCL 管理逻辑集中在一处，可独立测试和演进；
  - IOCTL 层变得简单、稳定，不会堆积与几何/碰撞相关的逻辑和状态。

- **风险**：
  - 分层重构过程中需要谨慎保持现有 API/IOCTL 行为不变，否则会影响现有上层使用；
  - 可能暴露出之前“隐藏在封装里的”设计缺陷，需要同步修正自测/健康检查逻辑。

- **与现有 changes 的关系**：
  - `port-fcl-to-kernel` / `update-use-upstream-fcl` 更偏向“把 FCL 带进内核并可靠使用”；
  - 本次 `refactor-thin-fcl-layer` 更偏向“在现有基础上，重整分层边界和封装厚度”，不改变 FCL 能力，只改变结构与职责划分。
