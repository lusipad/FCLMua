# 验收标准（Acceptance Criteria）

说明：以下标准用于判断“移植到 Windows 内核态的最小可用碰撞检测能力”是否达标。标准需满足：可观测、可重复、可自动化。

## A. 初始化与清理
- 驱动加载后，调用 `FclInitialize()` 返回 `STATUS_SUCCESS`；重复调用返回 `STATUS_ALREADY_INITIALIZED` 且不改变状态。
- 卸载前调用 `FclCleanup()`，使用池标签 `"FCL "` 统计验证无泄漏（分配=释放）。

## B. 几何体管理
- 能成功创建 Sphere/OBB/Mesh 至少各 1 种；参数非法时返回 `STATUS_INVALID_PARAMETER`；内存不足时返回 `STATUS_INSUFFICIENT_RESOURCES`；失败路径不泄漏。
- 有效句柄销毁返回 `STATUS_SUCCESS`；重复销毁返回 `STATUS_INVALID_HANDLE`。

## C. 碰撞检测
- Sphere-Sphere 相交与不相交各 1 个成功用例，返回 `STATUS_SUCCESS`，布尔结果正确。
- 边界接触（距离=0）有明确约定并在文档声明，测试用例与实现一致。

## D. IOCTL 与可观测性
- 提供 `IOCTL_FCL_PING` 返回版本与健康状态（已初始化、内存统计、上次错误码）。
- 所有公开 API 在 `PASSIVE_LEVEL` 下可用；涉及浮点的路径在高 IRQL 时保存/恢复浮点状态（如适用）。

## E. 异常与错误码
- 内部异常不越过边界；统一转译为 `NTSTATUS` 并记录错误日志（可通过调试输出或环形缓冲查询）。

## F. 数学内核与分配器
- Eigen 在禁对齐与自定义分配（NonPagedPool）下通过基础线性代数自测：向量/矩阵加减乘、转置、范数。
- 禁用不可用的 CRT/静态初始化路径；不依赖进程地址空间特性。

## G. 自动化与脚本化
- 提供最小自动化脚本/步骤（WinDbg/CI）能够：加载驱动、调用自检 IOCTL、运行最小几何+碰撞用例、收集并断言状态、卸载驱动并核对无泄漏。

