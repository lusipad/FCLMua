# 规范：内核态碰撞检测（Collision Detection）

本文档定义了 FCL+Musa 驱动的内核态 3D 碰撞检测功能规范。

## ADDED Requirements

### Requirement: 错误码与异常转译

系统 SHALL 使用 NTSTATUS 作为边界错误码；内部 C++ 异常在 API 边界统一转译为 NTSTATUS 并记录日志。

#### Scenario: 参数非法

- WHEN API 收到非法参数（空指针/越界/无效枚举）
- THEN 返回 `STATUS_INVALID_PARAMETER`
- AND 不分配任何资源

#### Scenario: 句柄无效

- WHEN API 收到无效句柄或已销毁句柄
- THEN 返回 `STATUS_INVALID_HANDLE`
- AND 不改变现有有效状态

#### Scenario: 资源不足

- WHEN 内部分配失败（NonPagedPool 不足）
- THEN 返回 `STATUS_INSUFFICIENT_RESOURCES`
- AND 已分配资源被正确回滚并记录

#### Scenario: 内部异常

- WHEN 内部出现未预期异常
- THEN 转译为 `STATUS_INTERNAL_ERROR` 并记录错误日志

---

### Requirement: 运行时约束（IRQL/内存）

系统 SHALL 遵循 PASSIVE_LEVEL 边界并仅使用 NonPagedPool；必要时在高 IRQL 路径保存/恢复浮点状态。

#### Scenario: IRQL 约束

- WHEN 在 DISPATCH_LEVEL 执行涉及浮点的路径
- THEN 使用 `KeSaveFloatingPointState`/`KeRestoreFloatingPointState`
- AND 不在更高 IRQL 进入不可重入路径

#### Scenario: 内存池与标签

- WHEN 进行分配/释放
- THEN 仅使用 NonPagedPool 并打 `"FCL "` 池标签
- AND 可通过统计验证分配=释放

---

### Requirement: IOCTL 自检

系统 SHALL 提供最小 IOCTL 通路用于观测：查询版本、初始化状态、上次错误、内存统计。

#### Scenario: IOCTL_FCL_PING

- WHEN 发送 `IOCTL_FCL_PING`
- THEN 返回版本、已初始化标志、上次错误码、当前池统计
- AND 返回 `STATUS_SUCCESS`

### Requirement: 驱动初始化和清理

驱动 **SHALL** 提供初始化和清理函数，用于设置和释放全局资源。

#### Scenario: 驱动加载成功

- **WHEN** 驱动加载时调用 `FclInitialize()`
- **THEN** 函数返回 `STATUS_SUCCESS`
- **AND** 驱动已准备好处理请求

#### Scenario: 驱动卸载清理

- **WHEN** 驱动卸载时调用 `FclCleanup()`
- **THEN** 所有分配的资源被释放
- **AND** 无内存泄漏

#### Scenario: 重复初始化

- **WHEN** 多次调用 `FclInitialize()`
- **THEN** 函数返回 `STATUS_ALREADY_INITIALIZED`
- **AND** 不影响已有的初始化状态

---

### Requirement: 几何对象创建

系统 **SHALL** 支持创建网格、OBB、球体等几何，保证句柄唯一、失败路径可重入并使用 NonPagedPool。

#### Scenario: 创建三角网格对象

- **WHEN** 调用 `FclCreateGeometry()` 且 `type=FCL_GEOMETRY_MESH`
- **AND** 顶点、索引缓冲均位于 NonPagedPool 并通过校验
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** 生成可在任意线程使用的几何句柄

#### Scenario: 创建 OBB 包围盒

- **WHEN** 调用 `FclCreateGeometry()` 且 `type=FCL_GEOMETRY_OBB`
- **AND** 提供中心、半轴和正交变换矩阵
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** 句柄可直接传入碰撞/距离 API

#### Scenario: 创建球体

- **WHEN** 调用 `FclCreateGeometry()` 且 `type=FCL_GEOMETRY_SPHERE`
- **AND** 半径>0 且参数来源受信任
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** 句柄可用于自检 IOCTL

#### Scenario: 参数验证失败

- **WHEN** 传入零半径、负体积或非法指针
- **THEN** 返回 `STATUS_INVALID_PARAMETER`
- **AND** 不分配任何持久资源

#### Scenario: 内存分配失败

- **WHEN** NonPagedPool 无可用空间
- **THEN** 返回 `STATUS_INSUFFICIENT_RESOURCES`
- **AND** 所有已分配缓冲被回滚

#### Scenario: 句柄唯一性

- **WHEN** 连续两次用相同参数创建几何
- **THEN** 返回不同的 `FCL_GEOMETRY_HANDLE`
- **AND** 彼此独立销毁不会互相影响

#### Scenario: 失败回滚

- **WHEN** 在构建网格 BVH 中途失败
- **THEN** 返回具体错误码
- **AND** 池标签统计与调用前一致

#### Scenario: 并发创建

- **WHEN** 多个线程同时调用 `FclCreateGeometry()`
- **THEN** 每次调用保持线程安全
- **AND** 不依赖全局锁造成串行化

---

### Requirement: 几何对象销毁

系统 **SHALL** 提供幂等且可重入的几何销毁 API，确保引用计数归零后释放 NonPagedPool 资源。

#### Scenario: 销毁有效对象

- **WHEN** 调用 `FclDestroyGeometry()` 并传入有效句柄
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** 释放所有关联内存
- **AND** 句柄立即标记为无效

#### Scenario: 句柄无效

- **WHEN** 传入未初始化或已销毁的句柄
- **THEN** 返回 `STATUS_INVALID_HANDLE`
- **AND** 不影响其他对象

#### Scenario: 重复销毁

- **WHEN** 对同一对象多次调用 `FclDestroyGeometry()`
- **THEN** 首次返回 `STATUS_SUCCESS`
- **AND** 随后返回 `STATUS_INVALID_HANDLE`

#### Scenario: 对象正在使用

- **WHEN** 句柄仍被碰撞/距离计算引用
- **THEN** `FclDestroyGeometry()` 返回 `STATUS_DEVICE_BUSY`
- **AND** 日志记录当前持有者以便重试

#### Scenario: 并发销毁

- **WHEN** 多线程并行销毁不同句柄
- **THEN** 每个调用都独立返回 `STATUS_SUCCESS`
- **AND** 池标签统计与预期一致

#### Scenario: 清理异常回滚

- **WHEN** 销毁过程中抛出内部异常
- **THEN** API 捕获并返回 `STATUS_INTERNAL_ERROR`
- **AND** 已释放资源不被重复释放

---

### Requirement: 碰撞检测

系统 **SHALL** 提供静态碰撞检测，至少覆盖球-球、网格-OBB 等组合，并在未初始化或高 IRQL 条件下给出明确反馈。

#### Scenario: 球-球相交

- **WHEN** `FclCollisionDetect()` 接收两个球体且中心距离 < r1+r2
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** `isColliding=TRUE`
- **AND** 可选接触信息包含接触点/法线

#### Scenario: 球-球不相交

- **WHEN** 中心距离 > r1+r2
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** `isColliding=FALSE`

#### Scenario: 球-球边界接触

- **WHEN** 中心距离 == r1+r2
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** 根据配置将结果标记为“接触”并输出一致的布尔值

#### Scenario: 网格-OBB 检测

- **WHEN** 一个三角网格与 OBB 组合进行检测
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** 结果与实际几何关系一致

#### Scenario: 返回接触信息

- **WHEN** 调用方提供接触点缓冲
- **THEN** 在碰撞时填充点、法线、穿透深度
- **AND** 在无碰撞时返回空集合

#### Scenario: 未初始化调用

- **WHEN** 在 `FclInitialize()` 之前调用 `FclCollisionDetect()`
- **THEN** 返回 `STATUS_DEVICE_NOT_READY`
- **AND** 记录错误日志

#### Scenario: IRQL 约束

- **WHEN** 在 DISPATCH_LEVEL 触发碰撞检测
- **THEN** 保存/恢复浮点状态
- **AND** 不访问分页内存

#### Scenario: 多线程并发

- **WHEN** 多个线程同时调用 `FclCollisionDetect()`
- **THEN** 每次调用互不影响，结果可重入

#### Scenario: 批量性能

- **WHEN** 连续执行 1000 次球-球检测
- **THEN** 平均耗时 < 0.01ms（PASSIVE_LEVEL）

#### Scenario: 单次时延

- **WHEN** 执行单次简单碰撞检测
- **THEN** 总耗时 < 1ms，用于实时验证

#### Scenario: 带接触信息的碰撞检测

- **WHEN** 调用 `FclCollisionDetect()` 并请求接触点信息
- **AND** 两个对象发生碰撞
- **THEN** 函数返回 `STATUS_SUCCESS`
- **AND** 输出接触点位置、法向量和穿透深度

#### Scenario: IRQL 级别检查

- **WHEN** 在 DISPATCH_LEVEL 或更低 IRQL 调用 `FclCollisionDetect()`
- **THEN** 函数正常执行
- **AND** 正确保存和恢复浮点状态

#### Scenario: 性能要求

- **WHEN** 检测简单几何对象（球体、OBB）
- **THEN** 单次检测耗时 **SHALL** 小于 1ms

#### Scenario: 复杂网格性能

- **WHEN** 检测包含 1000 个三角形的网格
- **THEN** 单次检测耗时 **SHALL** 小于 10ms

---

### Requirement: 距离计算

系统 **SHALL** 提供距离计算功能，输出最小距离、最近点对以及穿透深度，满足内核态精度要求。

#### Scenario: 球-球分离距离

- **WHEN** 两个球体中心距离 > r1+r2
- **THEN** `FclDistanceCompute()` 返回 `STATUS_SUCCESS`
- **AND** 距离值等于 `|d - (r1+r2)|`
- **AND** 最近点位于球面上

#### Scenario: 球-球穿透

- **WHEN** 中心距离 < r1+r2
- **THEN** 返回 `STATUS_SUCCESS`
- **AND** 距离值为负数，绝对值等于穿透深度

#### Scenario: 网格-OBB 距离

- **WHEN** 一个网格与 OBB 计算距离
- **THEN** 输出最近点坐标和法线
- **AND** 误差在 0.01% 以内

#### Scenario: 最近点信息

- **WHEN** 调用方提供最近点输出缓冲
- **THEN** API 填充两个对象上的最近点和对应法线

#### Scenario: 未初始化

- **WHEN** 在 `FclInitialize()` 前调用 `FclDistanceCompute()`
- **THEN** 返回 `STATUS_DEVICE_NOT_READY`
- **AND** 记录错误日志

#### Scenario: 距离计算精度

- **WHEN** 进行任意距离计算
- **THEN** 数值误差 **SHALL** 小于 0.01% 且与单元测试一致

---

### Requirement: 变换支持

驱动 **SHALL** 支持对几何对象进行平移、旋转和缩放变换。

#### Scenario: 应用平移变换

- **WHEN** 提供平移向量作为变换参数
- **THEN** 碰撞检测在变换后的位置进行
- **AND** 原始几何对象不被修改

#### Scenario: 应用旋转变换

- **WHEN** 提供旋转矩阵或四元数作为变换参数
- **THEN** 碰撞检测在旋转后的姿态进行
- **AND** 原始几何对象不被修改

#### Scenario: 组合变换

- **WHEN** 提供包含平移、旋转和缩放的复合变换
- **THEN** 碰撞检测正确应用所有变换
- **AND** 变换顺序为：缩放 → 旋转 → 平移

---

### Requirement: 内存管理

驱动 **SHALL** 使用非分页内存池（NonPagedPool）进行内存分配，并确保无内存泄漏。

#### Scenario: 内存分配使用正确的池类型

- **WHEN** 驱动分配内存
- **THEN** 使用 `ExAllocatePool2()` 并指定 `POOL_FLAG_NON_PAGED`
- **AND** 使用唯一的内存标签（如 `'FCL '`）

#### Scenario: 内存释放

- **WHEN** 对象被销毁或驱动卸载
- **THEN** 所有分配的内存被释放
- **AND** 使用 `ExFreePoolWithTag()` 释放

#### Scenario: 内存泄漏检测

- **WHEN** 驱动卸载后运行 Driver Verifier
- **THEN** 不报告任何内存泄漏

#### Scenario: 内存分配失败处理

- **WHEN** 内存分配失败（返回 NULL）
- **THEN** 函数优雅地返回错误状态
- **AND** 不导致系统崩溃

---

### Requirement: 线程安全

驱动 **SHALL** 确保多线程环境下的线程安全，允许并发访问。

#### Scenario: 并发对象创建

- **WHEN** 多个线程同时调用 `FclCreateGeometry()`
- **THEN** 所有调用正确完成
- **AND** 每个线程获得独立的对象句柄
- **AND** 无数据竞争

#### Scenario: 并发碰撞检测

- **WHEN** 多个线程同时调用 `FclCollisionDetect()`
- **THEN** 所有调用正确完成
- **AND** 结果正确且独立
- **AND** 无数据竞争

#### Scenario: 并发对象销毁

- **WHEN** 多个线程同时销毁不同的对象
- **THEN** 所有调用正确完成
- **AND** 无资源冲突

---

### Requirement: 错误处理

驱动 **SHALL** 使用 NTSTATUS 返回码报告错误，并在内部捕获所有异常。

#### Scenario: 参数验证错误

- **WHEN** 提供无效参数（空指针、越界值等）
- **THEN** 函数返回 `STATUS_INVALID_PARAMETER`
- **AND** 不执行任何操作

#### Scenario: 内部异常捕获

- **WHEN** 内部 C++ 代码抛出异常
- **THEN** 异常被捕获并转换为 `STATUS_INTERNAL_ERROR`
- **AND** 不导致内核崩溃

#### Scenario: 资源不足错误

- **WHEN** 系统资源不足（内存、句柄等）
- **THEN** 函数返回 `STATUS_INSUFFICIENT_RESOURCES`
- **AND** 已分配的资源被清理

#### Scenario: 错误日志记录

- **WHEN** 发生错误
- **THEN** 详细的错误信息被记录到内核日志
- **AND** 包含错误码、函数名和参数信息

---

### Requirement: 性能监控

驱动 **SHALL** 提供性能监控功能，记录关键操作的耗时和统计信息。

#### Scenario: 记录碰撞检测耗时

- **WHEN** 执行碰撞检测操作
- **THEN** 操作耗时被记录
- **AND** 可通过性能计数器查询

#### Scenario: 内存使用统计

- **WHEN** 查询驱动的内存使用情况
- **THEN** 返回当前分配的内存总量
- **AND** 返回对象数量统计

#### Scenario: 性能报告

- **WHEN** 查询性能统计
- **THEN** 返回总调用次数、平均耗时、最大耗时等信息

---

### Requirement: 兼容性

驱动 **SHALL** 兼容 Windows 10 (1809+) 和 Windows 11 的所有版本。

#### Scenario: Windows 10 兼容性

- **WHEN** 驱动在 Windows 10 (1809+) 上加载
- **THEN** 所有功能正常工作

#### Scenario: Windows 11 兼容性

- **WHEN** 驱动在 Windows 11 上加载
- **THEN** 所有功能正常工作

#### Scenario: 64 位架构

- **WHEN** 驱动在 x64 架构上运行
- **THEN** 所有功能正常工作

---

### Requirement: 驱动签名

发布版本的驱动 **SHALL** 经过 Microsoft 签名或使用测试签名（测试环境）。

#### Scenario: 生产环境签名

- **WHEN** 驱动用于生产环境
- **THEN** 驱动经过 Microsoft WHQL 签名
- **AND** 可以在非测试模式的 Windows 上加载

#### Scenario: 测试签名

- **WHEN** 驱动用于开发和测试
- **THEN** 驱动使用测试证书签名
- **AND** 可以在启用测试模式的 Windows 上加载

---

### Requirement: 调试支持

驱动 **SHALL** 提供调试功能，便于开发和故障排查。

#### Scenario: 调试日志

- **WHEN** 驱动在 Debug 配置下编译
- **THEN** 输出详细的调试日志到 DbgPrint
- **AND** 可通过 DebugView 或 WinDbg 查看

#### Scenario: 断言检查

- **WHEN** 驱动在 Debug 配置下运行
- **THEN** 启用所有断言检查
- **AND** 断言失败时触发断点

#### Scenario: 内存标签

- **WHEN** 查看内存分配
- **THEN** 所有分配使用 `'FCL '` 标签
- **AND** 可通过 `!poolfind FCL ` 命令查找
