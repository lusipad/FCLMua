# 技术设计文档：FCL 内核移植

## Context（背景）

FCL (Flexible Collision Library) 是一个成熟的 C++ 碰撞检测库，广泛用于机器人、计算机图形学等领域。然而，FCL 原本设计用于用户态，依赖标准 C++ STL 和外部库（Eigen、libccd）。为了在 Windows 内核驱动中使用碰撞检测功能，需要将 FCL 移植到内核模式。

### 约束条件
- 必须在 Windows 内核模式下运行（Ring 0）
- 遵守 IRQL 级别限制
- 不能使用标准 CRT 的某些函数
- 内存分配必须使用内核 API
- 异常处理需要特殊处理
- 不能导致系统崩溃（BSOD）

### 利益相关者
- 内核驱动开发者（需要碰撞检测功能）
- 系统稳定性（避免内核崩溃）
- 性能要求（实时碰撞检测）

## Goals / Non-Goals（目标与非目标）

### Goals（目标）
- ✅ 在内核模式下提供基本的碰撞检测功能
- ✅ 支持常见的几何对象（三角网格、OBB、AABB、球体等）
- ✅ 保持 API 接口的易用性
- ✅ 确保内存安全和资源管理
- ✅ 提供足够的性能满足实时需求

### Non-Goals（非目标）
- ❌ 不支持 FCL 的所有高级功能（如动画、形变等）
- ❌ 不追求与用户态版本 100% 功能一致
- ❌ 不支持所有 Eigen 的高级功能（如复数、稀疏矩阵等）
- ❌ 不提供用户态 API（仅内核态）

## Architecture Overview（架构概览）

```
┌─────────────────────────────────────────┐
│      用户态应用 (可选)                    │
└───────────────┬─────────────────────────┘
                │ IOCTL
┌───────────────┴─────────────────────────┐
│      FCL+Musa 驱动层                     │
├─────────────────────────────────────────┤
│  API 接口层                              │
│  - IOCTL Handler                        │
│  - 参数验证                              │
│  - 资源管理                              │
├─────────────────────────────────────────┤
│  FCL 核心层                              │
│  - 碰撞检测算法                          │
│  - 距离计算算法                          │
│  - BVH 数据结构                          │
├─────────────────────────────────────────┤
│  数学库层 (Eigen 适配)                   │
│  - 向量/矩阵运算                         │
│  - 变换计算                              │
├─────────────────────────────────────────┤
│  Musa.Runtime (STL + 异常处理)          │
│  - std::vector, std::shared_ptr 等      │
│  - C++ 异常支持                          │
├─────────────────────────────────────────┤
│  内核兼容层                              │
│  - 内存分配器 (NonPagedPool)            │
│  - IRQL 管理                             │
│  - 错误处理                              │
└───────────────┬─────────────────────────┘
                │
┌───────────────┴─────────────────────────┐
│      Windows 内核 (ntoskrnl.exe)         │
└─────────────────────────────────────────┘
```

## Key Decisions（关键决策）

### Decision 1: 使用 Musa.Runtime 作为 STL 运行时

**决策**：使用 MiroKaku 的 Musa.Runtime（原 ucxxrt）提供内核态 C++ STL 支持。

**理由**：
- Musa.Runtime 专为 Windows 内核设计，成熟稳定
- 支持 C++ 异常处理（FCL 代码中有异常使用）
- 提供常用的 STL 容器（vector, map, shared_ptr 等）
- 开源且活跃维护
- 支持 NuGet 集成，易于使用

**备选方案**：
1. **手写 STL 实现**：工作量巨大，容易出错
2. **使用其他内核 STL 库（如 stlkrn）**：不如 Musa.Runtime 成熟
3. **避免使用 STL，重写 FCL**：开发时间太长

### Decision 2: 选择性移植 FCL 功能

**决策**：优先移植核心碰撞检测功能，暂不支持高级特性。

**移植功能**：
- ✅ 基本碰撞检测（两个对象是否碰撞）
- ✅ 距离计算（两个对象的最小距离）
- ✅ 常见几何对象（三角网格、OBB、AABB、球体、胶囊体）
- ✅ BVH 加速结构

**暂不支持**：
- ❌ 连续碰撞检测（CCD）
- ❌ 宽相位批量检测（Broadphase）
- ❌ 高级查询（容差验证等）
- ❌ Octomap 八叉树支持

**理由**：
- 减少移植工作量
- 降低复杂度和风险
- 满足大部分使用场景
- 后续可根据需求逐步添加

### Decision 3: Eigen 库的适配策略

**决策**：使用 Eigen 的 header-only 特性，选择性启用功能。

**策略**：
- 使用 Eigen 3.x（header-only）
- 禁用依赖 C 标准库的功能（如 `std::cout`）
- 替换 Eigen 的内存分配为内核分配器
- 禁用多线程支持（内核有自己的同步机制）
- 避免使用浮点异常（`-fp:precise` 编译选项）

**配置宏**：
```cpp
#define EIGEN_NO_IO              // 禁用 iostream
#define EIGEN_NO_DEBUG           // 禁用 assert
#define EIGEN_NO_STATIC_ASSERT   // 禁用静态断言
#define EIGEN_DONT_ALIGN         // 禁用 SSE 对齐（内核内存分配不保证对齐）
#define EIGEN_MALLOC custom_malloc
#define EIGEN_FREE custom_free
```

**风险**：Eigen 可能有未知的 CRT 依赖，需要测试验证。

### Decision 4: libccd 的处理方式

**决策**：手动移植 libccd 核心算法到内核。

**理由**：
- libccd 代码量较小（约 2000 行）
- 核心算法简单（GJK、EPA）
- 避免引入额外的依赖问题

**实施**：
- 将 libccd 源码复制到项目中
- 修改内存分配为内核分配器
- 移除不兼容的代码（如 `fprintf` 等）
- 添加内核兼容的错误处理

### Decision 5: 内存管理策略

**决策**：使用非分页内存池（NonPagedPool），并封装为 RAII 风格。

**内存分配器**：
```cpp
void* operator new(size_t size) {
    return ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        size,
        'FCL '  // 内存标签
    );
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        ExFreePoolWithTag(ptr, 'FCL ');
    }
}
```

**RAII 包装器**：
- 使用 `std::unique_ptr` 管理对象生命周期
- 使用 `std::shared_ptr` 处理共享所有权（谨慎使用，避免循环引用）
- 自定义删除器处理特殊资源

**内存跟踪**：
- 使用内存标签（Pool Tag）便于调试
- 定期检查内存使用情况
- 在驱动卸载时验证无泄漏

### Decision 6: IRQL 级别管理

**决策**：大部分操作在 PASSIVE_LEVEL 执行，关键路径允许 DISPATCH_LEVEL。

**IRQL 策略**：
- **PASSIVE_LEVEL**：API 初始化、对象创建/销毁
- **DISPATCH_LEVEL**：碰撞检测查询、距离计算（性能关键）
- **不允许更高 IRQL**：避免引入复杂性

**浮点运算处理**：
```cpp
// 在 DISPATCH_LEVEL 使用浮点需要保存/恢复上下文
KFLOATING_SAVE floatSave;
NTSTATUS status = KeSaveFloatingPointState(&floatSave);
if (NT_SUCCESS(status)) {
    // 执行浮点运算（碰撞检测）
    KeRestoreFloatingPointState(&floatSave);
}
```

### Decision 7: 错误处理策略

**决策**：使用 NTSTATUS 返回码，避免抛出异常到内核边界。

**策略**：
- 内部使用 C++ 异常（Musa.Runtime 支持）
- 在 API 边界捕获所有异常，转换为 NTSTATUS
- 记录详细的错误日志
- 失败时确保资源清理

**示例**：
```cpp
NTSTATUS FclCollisionDetect(
    _In_ PVOID object1,
    _In_ PVOID object2,
    _Out_ PBOOLEAN isColliding
) {
    __try {
        // 调用 FCL C++ 代码
        bool result = fcl::collide(obj1, obj2);
        *isColliding = result;
        return STATUS_SUCCESS;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERROR("Exception in collision detection");
        return STATUS_INTERNAL_ERROR;
    }
}
```

## Data Model（数据模型）

### 核心对象

```cpp
// 几何对象基类
class FclGeometry {
    GeometryType type;
    void* internalData;
    // ...
};

// 碰撞对象（包含几何 + 变换）
class FclCollisionObject {
    std::shared_ptr<FclGeometry> geometry;
    Transform3f transform;
    // ...
};

// 碰撞请求参数
struct FclCollisionRequest {
    bool enableContact;        // 是否返回接触点
    size_t numMaxContacts;     // 最大接触点数
    // ...
};

// 碰撞结果
struct FclCollisionResult {
    bool isCollision;
    std::vector<Contact> contacts;
    // ...
};
```

## API Design（API 设计）

### 内核内部 API（供其他驱动模块调用）

```cpp
// 初始化
NTSTATUS FclInitialize();
VOID FclCleanup();

// 对象管理
NTSTATUS FclCreateGeometry(
    _In_ FCL_GEOMETRY_TYPE type,
    _In_ PVOID geometryData,
    _Out_ PFCL_GEOMETRY_HANDLE handle
);

NTSTATUS FclDestroyGeometry(
    _In_ FCL_GEOMETRY_HANDLE handle
);

// 碰撞检测
NTSTATUS FclCollisionDetect(
    _In_ FCL_GEOMETRY_HANDLE object1,
    _In_ PFCL_TRANSFORM transform1,
    _In_ FCL_GEOMETRY_HANDLE object2,
    _In_ PFCL_TRANSFORM transform2,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contacts
);

// 距离计算
NTSTATUS FclDistanceCompute(
    _In_ FCL_GEOMETRY_HANDLE object1,
    _In_ PFCL_TRANSFORM transform1,
    _In_ FCL_GEOMETRY_HANDLE object2,
    _In_ PFCL_TRANSFORM transform2,
    _Out_ PFLOAT distance
);
```

### IOCTL 接口（可选，供用户态调用）

```cpp
// IOCTL 控制码
#define IOCTL_FCL_CREATE_GEOMETRY   CTL_CODE(...)
#define IOCTL_FCL_DESTROY_GEOMETRY  CTL_CODE(...)
#define IOCTL_FCL_COLLISION_DETECT  CTL_CODE(...)
#define IOCTL_FCL_DISTANCE_COMPUTE  CTL_CODE(...)
```

## Performance Considerations（性能考虑）

### 优化目标
- 单次碰撞检测 < 1ms（简单几何对象）
- 单次碰撞检测 < 10ms（复杂三角网格）
- 内存开销 < 100MB（包括 BVH 数据结构）

### 优化策略
1. **内存池复用**：避免频繁分配/释放
2. **BVH 预计算**：静态对象预先构建 BVH
3. **SSE/AVX 优化**：利用 SIMD 加速向量运算（如果 Eigen 支持）
4. **减少拷贝**：使用引用和移动语义
5. **缓存友好**：数据结构紧凑排列

### 性能监控
- 添加性能计数器（ETW）
- 记录每次调用的耗时
- 定期报告性能统计

## Security Considerations（安全考虑）

### 输入验证
- 验证所有用户态传入的参数
- 检查指针有效性（ProbeForRead/ProbeForWrite）
- 验证缓冲区大小
- 防止整数溢出

### 资源限制
- 限制同时创建的对象数量
- 限制单个对象的内存占用
- 防止资源耗尽攻击

### 权限控制
- 仅允许管理员权限访问（可选）
- 记录关键操作日志

## Testing Strategy（测试策略）

### 单元测试
- 数学库测试（向量、矩阵运算）
- 几何对象创建和销毁
- 基本碰撞检测算法

### 集成测试
- 多对象碰撞场景
- 复杂网格碰撞
- 边界情况（空对象、重叠对象等）

### 压力测试
- 大量并发请求
- 内存泄漏检测
- 长时间运行稳定性

### 性能测试
- 基准测试套件
- 与用户态版本对比
- 不同几何对象性能对比

## Migration Plan（迁移计划）

### 阶段 1：环境搭建（1-2 周）
- 安装 WDK 和配置环境
- 集成 Musa.Runtime
- 创建基本驱动框架

### 阶段 2：依赖库移植（3-4 周）
- Eigen 适配和测试
- libccd 移植
- 验证基本数学运算

### 阶段 3：FCL 核心移植（3-4 周）
- 移植几何对象定义
- 移植碰撞检测算法
- API 接口实现

### 阶段 4：测试和优化（2-3 周）
- 功能测试
- 性能优化
- 稳定性测试

### 回滚计划
- 每个阶段完成后创建虚拟机快照
- 保持详细的变更日志
- 关键代码保留原始版本备份

## Open Questions（未决问题）

1. **Eigen 的 SSE/AVX 优化在内核中是否可用？**
   - 需要测试验证
   - 可能需要禁用 SIMD 优化

2. **Musa.Runtime 的异常处理性能如何？**
   - 需要性能测试
   - 考虑是否使用错误码代替异常

3. **是否需要支持多线程碰撞检测？**
   - 内核有自己的线程模型
   - 需要评估同步开销

4. **是否需要提供用户态 API？**
   - 决定是否实现 IOCTL 接口
   - 用户态封装库的设计

5. **如何处理 FCL 的动态内存分配高峰？**
   - 考虑预分配策略
   - 内存池设计

## Risks and Mitigations（风险与缓解）

### 风险 1：Musa.Runtime 不兼容某些 FCL 代码
- **风险级别**：高
- **缓解措施**：
  - 提前测试 Musa.Runtime 的 STL 兼容性
  - 准备手写关键组件的备选方案
  - 与 Musa.Runtime 作者沟通

### 风险 2：性能不满足实时要求
- **风险级别**：中
- **缓解措施**：
  - 提前进行性能基准测试
  - 识别热点路径并优化
  - 考虑使用更简单的算法

### 风险 3：内存泄漏导致系统崩溃
- **风险级别**：高
- **缓解措施**：
  - 严格的代码审查
  - 使用 Driver Verifier 检测
  - RAII 风格管理资源
  - 详细的日志记录

### 风险 4：浮点运算在高 IRQL 导致问题
- **风险级别**：中
- **缓解措施**：
  - 限制在 PASSIVE_LEVEL 和 DISPATCH_LEVEL
  - 正确保存/恢复浮点状态
  - 考虑定点数代替浮点数

### 风险 5：FCL 上游更新难以合并
- **风险级别**：低
- **缓解措施**：
  - 保持移植代码的模块化
  - 记录所有修改点
  - 定期同步上游变更

## References（参考资料）

- [FCL 官方文档](https://flexible-collision-library.github.io/)
- [FCL GitHub 仓库](https://github.com/flexible-collision-library/fcl)
- [Musa.Runtime GitHub 仓库](https://github.com/MiroKaku/Musa.Runtime)
- [Windows Driver Development 文档](https://learn.microsoft.com/en-us/windows-hardware/drivers/)
- [Eigen 官方文档](https://eigen.tuxfamily.org/)
- [libccd GitHub 仓库](https://github.com/danfis/libccd)
