# refactor-thin-fcl-layer 任务清单

## 1. 分层与接口梳理
- [x] 1.1 盘点当前 FCL 相关头文件和源文件（geometry/collision/distance/continuous_collision/self_test 等）
- [x] 1.2 明确每个 API 属于哪一层：upstream FCL / FCL 管理模块 / IOCTL 封装
- [x] 1.3 补充或更新架构文档中的分层说明（docs/architecture.md 已更新为三层结构）

## 2. 收敛 FCL 管理模块职责
- [x] 2.1 将几何管理和句柄生命周期逻辑统一收敛为“FCL 管理模块”的对外 API（核心代码迁移至 kernel/core/src）
- [x] 2.2 确保碰撞/距离/CCD API 入口只依赖 FCL 管理模块和 upstream FCL，不感知 IOCTL（已确认 collision/distance/continuous_collision 仅通过几何句柄 + upstream bridge 工作）
- [x] 2.3 检查 BVH 构建/更新实现，对可直接复用 upstream FCL 能力的部分做记录（已在架构文档中说明，后续如需可独立优化）

## 3. 瘦身 IOCTL 封装层
- [x] 3.1 审查 `device_control.cpp` 中各个 `Handle*` 函数，确保只包含 IRP 处理 + buffer 校验 + 调用 FCL 管理模块
- [x] 3.2 确认 IOCTL 层不再新增几何/碰撞/CCD 相关控制逻辑和状态管理（仅串联 FCL API，不维护持久状态）
- [x] 3.3 在 `device_control.cpp` 顶部添加简要注释，约束“IOCTL 层只做编解码 + 调用 FCL 管理 API，不承载业务逻辑”

## 4. 文档与自测
- [x] 4.1 更新 `docs/architecture.md`：加入三层分层和典型调用链说明，路径已指向 kernel/core/src 与 kernel/core/include
- [x] 4.2 更新 `docs/api.md`：从“FCL 管理模块 API + IOCTL 封装”角度梳理接口，并说明 IOCTL 层只做薄封装
- [ ] 4.3 运行或补充自测/健康检查，确认重构后行为与现有预期保持一致（需要在目标机器上通过 IOCTL_FCL_SELF_TEST / IOCTL_FCL_PING 等进行运行时验证）

