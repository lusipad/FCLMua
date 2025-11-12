# Collision Detection – 补充验收要点

此文件补充 `spec.md` 的“ADDED Requirements”对应的可操作验收点，用于对齐实现与测试。

## 初始化/清理
- FclInitialize：首次成功、重复初始化返回 `STATUS_ALREADY_INITIALIZED`；状态机线程安全。
- FclCleanup：释放全部资源，池标签 `"FCL "` 统计归零。

## 几何体
- 支持 Sphere/OBB/Mesh 的最小化建模与参数校验；失败路径不泄漏。
- Destroy 幂等：首次成功、再次返回 `STATUS_INVALID_HANDLE`。

## 碰撞
- Sphere-Sphere：相交、不相交、边界接触 3 组用例；统一 `NTSTATUS`；布尔结果准确。

## 运行时约束
- 公共 API 在 `PASSIVE_LEVEL`；若在更高 IRQL 涉及浮点，需保存/恢复状态。
- 内存：NonPagedPool + Pool Tag；RAII 管理；无隐式页池分配。

## 观测与调试
- IOCTL_FCL_PING：返回版本、初始化状态、上次错误、内存统计。
- 失败路径统一记录错误日志并可读取。

