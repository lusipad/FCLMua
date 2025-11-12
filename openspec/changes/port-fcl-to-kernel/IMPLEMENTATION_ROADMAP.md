# FCL 完整内核移植 - 实施路线图

## 🎯 项目概览

**目标**：将 FCL (27,714 行代码) 完整移植到 Windows 内核驱动，包括连续碰撞检测。

**时间**：36-49 周（9-12 个月）
**风险**：🔴 极高
**成功率**：< 30%

---

## ⚠️ 关键成功因素

在开始前，确保满足以下条件：

### 1. 团队要求
- ✅ 至少 1 名 Windows 内核驱动专家（5+ 年经验）
- ✅ 至少 1 名数值计算/计算几何专家
- ✅ 至少 1 名 C++ 模板元编程专家
- ✅ 全职投入 9-12 个月

### 2. 资源要求
- ✅ 高性能开发机器（32GB+ RAM，快速 SSD）
- ✅ 多台测试机器/虚拟机（用于内核测试）
- ✅ WinDbg 和 Driver Verifier 环境
- ✅ 持续集成环境（自动化测试）

### 3. 时间和预算
- ✅ 管理层支持 12 个月项目周期
- ✅ 接受 30% 的失败风险
- ✅ 有降级方案的预算（如失败转方案 C）

---

## 📋 阶段 0：风险验证（第 1-4 周）⭐ **最关键**

**目标**：验证关键技术可行性，早期识别阻断性问题

### 第 1-2 周：Musa.Runtime 深度验证

#### 任务清单
- [ ] 安装 Musa.Runtime（NuGet 或源码编译）
- [ ] 创建最小内核驱动项目（DriverEntry）
- [ ] 验证 STL 容器功能

#### 测试项目（创建 PoC-01-MusaRuntime）

```cpp
// 测试 1：基本容器
std::vector<int> vec;
vec.push_back(42);
ASSERT(vec.size() == 1);

// 测试 2：智能指针
auto ptr = std::make_shared<int>(100);
auto ptr2 = ptr;
ASSERT(ptr.use_count() == 2);

// 测试 3：std::map
std::map<int, std::string> map;
map[1] = "test";
ASSERT(map.size() == 1);

// 测试 4：std::function
std::function<int(int)> func = [](int x) { return x * 2; };
ASSERT(func(5) == 10);

// 测试 5：异常处理
try {
    throw std::runtime_error("test");
} catch (const std::exception& e) {
    // 捕获成功
}

// 测试 6：std::unordered_map
std::unordered_map<int, int> umap;
umap[1] = 42;
ASSERT(umap[1] == 42);
```

#### 验收标准
- ✅ 所有测试通过
- ✅ 无内存泄漏（Driver Verifier 验证）
- ✅ 异常处理正常工作
- ❌ **如果失败 → 项目终止或降级方案**

---

### 第 3-4 周：数学函数库可行性验证 🔴 **关键里程碑**

#### 背景
CCD 的泰勒模型需要以下数学函数：
- `sin`, `cos`, `tan`
- `exp`, `log`
- `sqrt`, `pow`
- `atan2`

内核没有 `<cmath>` 库，必须自己实现。

#### 方案评估

##### 方案 A：移植 OpenLibm
```
OpenLibm 是开源数学库，纯 C 实现
代码量：约 15,000 行
许可证：MIT（商业友好）
```

**任务**：
- [ ] 下载 OpenLibm 源码
- [ ] 识别需要的函数（sin, cos, exp, log, sqrt）
- [ ] 尝试编译到内核环境
- [ ] 测试精度和性能

##### 方案 B：使用 fdlibm（推荐）
```
fdlibm 是 Sun Microsystems 的数学库
代码量：约 8,000 行
许可证：公有领域
质量：高，被 Java 使用
```

**任务**：
- [ ] 下载 fdlibm 源码
- [ ] 适配到内核（移除 stdio.h 等）
- [ ] 实现内核兼容的错误处理
- [ ] 性能基准测试

##### 方案 C：自己实现（不推荐）
```
使用泰勒级数或 CORDIC 算法实现
工作量：4-6 周
精度风险：高
```

#### 测试项目（创建 PoC-02-MathLib）

```cpp
// 精度测试
double x = 0.5;
double sin_x = kernel_sin(x);
double expected = 0.479425538604203;
ASSERT(fabs(sin_x - expected) < 1e-10);

// 性能测试
LARGE_INTEGER start, end, freq;
KeQueryPerformanceCounter(&start);
for (int i = 0; i < 10000; i++) {
    kernel_sin(i * 0.001);
}
KeQueryPerformanceCounter(&end);
QueryPerformanceFrequency(&freq);
double elapsed_ms = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
// 目标：< 10ms（平均 1μs/次）
```

#### 验收标准
- ✅ 所有数学函数可用且精度满足（误差 < 1e-10）
- ✅ 性能可接受（sin/cos < 1μs，exp/log < 2μs）
- ✅ 无内存泄漏
- ❌ **如果失败 → 项目降级或终止**

#### 决策点 1（第 4 周末）

**Go / No-Go 决策**：

```
如果通过：
  → 继续阶段 1（基础设施）

如果失败：
  选项 A：降级到方案 B（简化 CCD，无泰勒模型）
  选项 B：降级到方案 C（用户态 CCD）
  选项 C：项目暂停，重新评估
```

---

## 📋 阶段 1：基础设施（第 5-12 周）

### 第 5-6 周：环境搭建

#### 任务清单
- [ ] 安装 WDK（Windows Driver Kit）
- [ ] 配置 Visual Studio 驱动项目
- [ ] 集成 Musa.Runtime
- [ ] 配置测试虚拟机（Hyper-V）
- [ ] 配置 WinDbg 内核调试
- [ ] 创建基本驱动框架

#### 驱动项目结构

```
FCL-Musa-Driver/
├── src/
│   ├── driver/
│   │   ├── DriverEntry.cpp
│   │   ├── DeviceControl.cpp
│   │   └── Unload.cpp
│   ├── runtime/              # Musa.Runtime 封装
│   │   ├── Memory.cpp        # 内存管理
│   │   └── Exception.cpp     # 异常处理
│   ├── math/                 # 数学库
│   │   ├── libm/             # fdlibm 适配
│   │   └── Wrappers.cpp
│   ├── eigen/                # Eigen 适配
│   ├── libccd/               # libccd 移植
│   ├── fcl/                  # FCL 核心
│   │   ├── collision/
│   │   ├── distance/
│   │   ├── ccd/
│   │   └── geometry/
│   └── api/                  # 对外 API
├── include/
├── test/                     # 测试驱动和用户态测试
└── docs/
```

---

### 第 7-10 周：数学函数库实现

#### 任务：集成 fdlibm

##### 7.1 适配 fdlibm 到内核
- [ ] 下载 fdlibm 源码
- [ ] 移除所有 stdio.h 依赖
- [ ] 替换 malloc/free 为 ExAllocatePool/ExFreePool
- [ ] 移除 errno（使用返回值或异常）
- [ ] 禁用所有 printf/fprintf

##### 7.2 实现核心函数
- [ ] `k_sin.c`, `k_cos.c` - 三角函数
- [ ] `e_exp.c` - 指数函数
- [ ] `e_log.c` - 对数函数
- [ ] `e_sqrt.c` - 平方根
- [ ] `e_pow.c` - 幂函数
- [ ] `e_atan2.c` - 反正切函数

##### 7.3 包装为 C++ 接口

```cpp
// fcl/math/kernel_math.h
namespace fcl {
namespace kernel_math {

inline double sin(double x) noexcept {
    return ::__kernel_sin(x, 0, 0);
}

inline double cos(double x) noexcept {
    return ::__kernel_cos(x, 0);
}

// ...其他函数
}
}
```

##### 7.4 单元测试
- [ ] 编写测试用例（100+ 个测试点）
- [ ] 验证精度（与标准库对比）
- [ ] 性能基准测试
- [ ] 边界情况测试（NaN, Inf, 极大/极小值）

#### 验收标准
- ✅ 所有数学函数通过测试
- ✅ 精度误差 < 1e-10
- ✅ 性能满足要求（见上文）

---

### 第 11-12 周：内存管理系统

#### 背景
FCL 有 229 处 new/delete，需要统一的内存管理。

#### 任务

##### 11.1 全局内存分配器
```cpp
// 重载全局 new/delete
void* operator new(size_t size) {
    return ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        size,
        'FCL '
    );
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        ExFreePoolWithTag(ptr, 'FCL ');
    }
}

// 数组版本
void* operator new[](size_t size);
void operator delete[](void* ptr) noexcept;

// placement new（已有内存）
void* operator new(size_t, void* ptr) noexcept { return ptr; }
void operator delete(void*, void*) noexcept {}
```

##### 11.2 内存池（Look-aside List）

```cpp
// 为小对象优化
class MemoryPool {
    LOOKASIDE_LIST_EX lookaside_;
public:
    void* Allocate(size_t size);
    void Free(void* ptr);
};
```

##### 11.3 内存追踪和调试

```cpp
// Debug 版本记录所有分配
struct AllocationRecord {
    void* address;
    size_t size;
    const char* file;
    int line;
    PVOID backtrace[16];
};

// 宏包装
#ifdef DBG
#define FCL_NEW new(__FILE__, __LINE__)
#else
#define FCL_NEW new
#endif
```

##### 11.4 泄漏检测
- [ ] DriverUnload 时检查是否所有内存已释放
- [ ] 集成 Driver Verifier
- [ ] 实现内存使用统计

#### 验收标准
- ✅ 内存分配/释放正常
- ✅ 无泄漏（Driver Verifier 验证）
- ✅ 支持内存追踪

---

## 📋 阶段 2：Eigen 和 libccd（第 13-24 周）

### 第 13-16 周：Eigen 核心适配

#### 13.1 Eigen 配置

```cpp
// fcl/eigen_config.h
#define EIGEN_NO_IO                 // 禁用 iostream
#define EIGEN_NO_DEBUG              // 禁用 assert
#define EIGEN_NO_STATIC_ASSERT      // 禁用静态断言
#define EIGEN_DONT_VECTORIZE        // 禁用 SIMD
#define EIGEN_DONT_ALIGN            // 禁用对齐要求
#define EIGEN_DONT_PARALLELIZE      // 禁用多线程
#define EIGEN_NO_AUTOMATIC_RESIZING // 禁用自动调整大小

// 重定向内存分配
#define EIGEN_MALLOC(size) ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'EIG ')
#define EIGEN_FREE(ptr) ExFreePoolWithTag(ptr, 'EIG ')
#define EIGEN_REALLOC(ptr, old_size, new_size) /* 自定义实现 */
```

#### 13.2 测试基本运算

```cpp
// 向量运算
Eigen::Vector3f v1(1, 2, 3);
Eigen::Vector3f v2(4, 5, 6);
Eigen::Vector3f v3 = v1 + v2;
ASSERT(v3[0] == 5 && v3[1] == 7 && v3[2] == 9);

// 矩阵运算
Eigen::Matrix3f m1 = Eigen::Matrix3f::Identity();
Eigen::Matrix3f m2 = Eigen::Matrix3f::Random();
Eigen::Matrix3f m3 = m1 * m2;

// 变换
Eigen::Affine3f transform = Eigen::Affine3f::Identity();
transform.translate(Eigen::Vector3f(1, 2, 3));
transform.rotate(Eigen::AngleAxisf(M_PI/4, Eigen::Vector3f::UnitZ()));
```

#### 13.3 修复编译错误
- [ ] 处理 Eigen 的 I/O 操作（移除或注释）
- [ ] 处理 assert（替换为 ASSERT 宏）
- [ ] 处理 std::runtime_error（替换为自定义异常）

---

### 第 17-20 周：Eigen 高级功能

#### 17.1 四元数
```cpp
Eigen::Quaternionf q1(1, 0, 0, 0);  // 单位四元数
Eigen::Quaternionf q2(0.707, 0, 0, 0.707);  // 绕 Z 轴旋转 90°
Eigen::Quaternionf q3 = q1 * q2;
```

#### 17.2 SLERP（球面线性插值）
```cpp
Eigen::Quaternionf slerp = q1.slerp(0.5, q2);
```

#### 17.3 特征值分解（如果需要）
```cpp
Eigen::Matrix3f m = ...;
Eigen::EigenSolver<Eigen::Matrix3f> solver(m);
auto eigenvalues = solver.eigenvalues();
```

#### 验收标准
- ✅ 所有 Eigen 测试通过
- ✅ 性能可接受（比用户态慢 < 50%）

---

### 第 21-24 周：libccd 移植

#### 21.1 下载和分析
- [ ] 克隆 libccd 源码
- [ ] 分析代码结构（约 2000-3000 行 C）
- [ ] 识别依赖项

#### 21.2 适配到内核
- [ ] 移除 stdio.h（printf, fprintf）
- [ ] 替换 malloc/free
- [ ] 移除 assert（替换为 ASSERT）
- [ ] 适配浮点数学函数（使用 fdlibm）

#### 21.3 测试 GJK/EPA 算法
- [ ] 球-球碰撞
- [ ] 盒-盒碰撞
- [ ] 凸多面体碰撞
- [ ] 距离计算

#### 验收标准
- ✅ libccd 编译通过
- ✅ GJK/EPA 测试通过

---

## 📋 阶段 3：静态碰撞检测（第 25-34 周）

### 第 25-28 周：几何对象

#### 任务
- [ ] 移植基本形状（Sphere, Box, Capsule, Cylinder, Cone）
- [ ] 移植 BVH 结构（AABB, OBB, RSS, kIOS）
- [ ] 移植 BVHModel（三角网格）

#### 文件清单（约 40 个文件）
```
geometry/
├── shape/
│   ├── sphere.h/.cpp
│   ├── box.h/.cpp
│   ├── capsule.h/.cpp
│   ├── cylinder.h/.cpp
│   ├── cone.h/.cpp
│   └── convex.h/.cpp
├── bvh/
│   ├── BVH_model.h/.cpp
│   ├── BVH_utility.h/.cpp
│   └── BV_fitter.h/.cpp
└── octree/ (可选)
```

---

### 第 29-32 周：碰撞检测核心

#### 任务
- [ ] 移植碰撞检测主函数（`collide()`）
- [ ] 移植形状-形状碰撞算法矩阵
- [ ] 移植 BVH 遍历节点
- [ ] 移植 GJK solver 集成

#### 关键文件
```
narrowphase/
├── collision.h/.cpp
├── collision_object.h/.cpp
├── collision_request.h/.cpp
├── collision_result.h/.cpp
└── detail/
    ├── traversal/
    │   └── collision/ (约 20 个文件)
    └── gjk_solver_libccd.h/.cpp
```

---

### 第 33-34 周：API 和测试

#### API 设计
```cpp
// 内核态 API
NTSTATUS FclInitialize();
VOID FclCleanup();

NTSTATUS FclCreateGeometry(
    FCL_GEOMETRY_TYPE type,
    PVOID geometryData,
    SIZE_T dataSize,
    PFCL_GEOMETRY_HANDLE pHandle
);

NTSTATUS FclDestroyGeometry(FCL_GEOMETRY_HANDLE handle);

NTSTATUS FclCollisionDetect(
    FCL_GEOMETRY_HANDLE object1,
    PFCL_TRANSFORM transform1,
    FCL_GEOMETRY_HANDLE object2,
    PFCL_TRANSFORM transform2,
    PFCL_COLLISION_REQUEST request,
    PFCL_COLLISION_RESULT result
);
```

#### 测试
- [ ] 单元测试（50+ 测试用例）
- [ ] 集成测试
- [ ] 性能测试

#### 里程碑验证
- ✅ 静态碰撞检测完全可用
- ✅ 通过 Driver Verifier
- ✅ 性能满足要求（< 10ms）

---

## 📋 阶段 4：区间算术和泰勒模型（第 35-48 周）🔴 **最难**

### 第 35-38 周：区间算术核心

#### 背景
区间算术是 CCD 的数学基础，提供严格的误差边界。

#### 35.1 Interval 类实现

```cpp
template<typename S>
class Interval {
    S lower_;  // 下界
    S upper_;  // 上界

public:
    // 构造
    Interval(S value) : lower_(value), upper_(value) {}
    Interval(S lower, S upper) : lower_(lower), upper_(upper) {}

    // 基本运算（需要舍入控制）
    Interval operator+(const Interval& other) const;
    Interval operator-(const Interval& other) const;
    Interval operator*(const Interval& other) const;
    Interval operator/(const Interval& other) const;

    // 比较
    bool operator<(const Interval& other) const;
    bool contains(S value) const;
    bool overlaps(const Interval& other) const;

    // 工具
    S center() const { return (lower_ + upper_) / 2; }
    S radius() const { return (upper_ - lower_) / 2; }
};
```

#### 35.2 舍入模式控制

**关键挑战**：区间运算需要向下/向上舍入

```cpp
// x86/x64 FPU 控制
class RoundingMode {
    unsigned int old_control_;

public:
    enum Mode {
        DOWN,    // 向下舍入
        UP,      // 向上舍入
        NEAREST  // 最近舍入（默认）
    };

    RoundingMode(Mode mode) {
        old_control_ = _controlfp(0, 0);
        switch (mode) {
        case DOWN:
            _controlfp(_RC_DOWN, _MCW_RC);
            break;
        case UP:
            _controlfp(_RC_UP, _MCW_RC);
            break;
        case NEAREST:
            _controlfp(_RC_NEAR, _MCW_RC);
            break;
        }
    }

    ~RoundingMode() {
        _controlfp(old_control_, _MCW_RC);
    }
};
```

#### 35.3 区间加法

```cpp
Interval Interval::operator+(const Interval& other) const {
    S lower, upper;

    // 下界：向下舍入
    {
        RoundingMode rm(RoundingMode::DOWN);
        lower = lower_ + other.lower_;
    }

    // 上界：向上舍入
    {
        RoundingMode rm(RoundingMode::UP);
        upper = upper_ + other.upper_;
    }

    return Interval(lower, upper);
}
```

#### 测试
- [ ] 基本运算测试（+, -, *, /）
- [ ] 精度测试（验证保守性）
- [ ] 性能测试（应该比标量慢 3-5 倍）

---

### 第 39-42 周：区间数学函数 🔴 **极难**

#### 背景
需要实现区间版本的数学函数，保证严格的上下界。

#### 39.1 区间平方根

```cpp
Interval sqrt(const Interval& x) {
    if (x.upper() < 0) {
        // 错误：负数的平方根
        return Interval(NAN, NAN);
    }

    S lower, upper;

    // 下界
    {
        RoundingMode rm(RoundingMode::DOWN);
        lower = (x.lower() <= 0) ? 0 : kernel_math::sqrt(x.lower());
    }

    // 上界
    {
        RoundingMode rm(RoundingMode::UP);
        upper = kernel_math::sqrt(x.upper());
    }

    return Interval(lower, upper);
}
```

#### 39.2 区间正弦（复杂）

```cpp
Interval sin(const Interval& x) {
    // 检查区间是否包含 sin 的极值点
    // sin 在 π/2 + 2kπ 达到最大值 1
    // sin 在 -π/2 + 2kπ 达到最小值 -1

    // 简化情况：区间宽度 < 2π
    if (x.radius() >= constants<S>::pi()) {
        return Interval(-1, 1);  // 保守估计
    }

    // 计算端点
    S sin_lower = kernel_math::sin(x.lower());
    S sin_upper = kernel_math::sin(x.upper());

    // 检查极值点...
    // (复杂逻辑，约 50 行代码)

    return Interval(min_value, max_value);
}
```

#### 工作量评估
- `Interval sqrt()`：1-2 天
- `Interval sin()`, `cos()`, `tan()`：1 周（复杂）
- `Interval exp()`, `log()`：1 周
- `Interval pow()`, `atan2()`：1 周
- **测试和验证**：1-2 周

**总计**：4-6 周

---

### 第 43-46 周：泰勒模型

#### 43.1 TaylorModel 类

```cpp
template<typename S>
class TaylorModel {
    S coeffs_[4];              // c0, c1, c2, c3
    Interval<S> remainder_;    // 余项
    std::shared_ptr<TimeInterval<S>> time_;

public:
    // 基本运算
    TaylorModel operator+(const TaylorModel& other) const;
    TaylorModel operator-(const TaylorModel& other) const;
    TaylorModel operator*(const TaylorModel& other) const;

    // 数学函数
    friend TaylorModel sin(const TaylorModel& tm);
    friend TaylorModel cos(const TaylorModel& tm);

    // 求值
    Interval<S> evaluate(S t) const;
    S center() const;
    Interval<S> bound() const;
};
```

#### 43.2 泰勒模型乘法（复杂）

```cpp
// (c0 + c1*t + c2*t² + c3*t³ + [r1]) * (d0 + d1*t + d2*t² + d3*t³ + [r2])
// 需要展开多项式并计算余项

TaylorModel TaylorModel::operator*(const TaylorModel& other) const {
    // 多项式乘法（卷积）
    S new_coeffs[4];
    new_coeffs[0] = coeffs_[0] * other.coeffs_[0];
    new_coeffs[1] = coeffs_[0] * other.coeffs_[1] + coeffs_[1] * other.coeffs_[0];
    // ... (7 次乘法)

    // 高阶项被截断并加入余项
    Interval<S> high_order_terms = ...;  // 复杂计算

    // 余项传播
    Interval<S> new_remainder = ...;  // 复杂计算

    return TaylorModel(new_coeffs, new_remainder, time_);
}
```

#### 工作量评估
- `TaylorModel` 基本运算：1 周
- `TaylorVector`, `TaylorMatrix`：1-2 周
- 数学函数（sin, cos, exp）：2 周
- **测试和验证**：1-2 周

**总计**：6-8 周

---

### 第 47-48 周：泰勒模型验证

#### 测试
- [ ] 精度测试（与解析解对比）
- [ ] 保守性测试（真实值必须在区间内）
- [ ] 性能测试
- [ ] 数值稳定性测试

#### 决策点 4（第 48 周末）

**验证泰勒模型是否满足要求**：

```
如果通过：
  → 继续阶段 5（运动模型）

如果精度不足：
  选项 A：调整算法参数
  选项 B：降级到简化版（一阶泰勒模型）
  选项 C：放弃泰勒模型，使用更简单的方法
```

---

## 📋 阶段 5-9：CCD 集成和完成（第 49-81 周）

### 简要说明
由于篇幅限制，后续阶段的详细任务已在 `tasks.md` 中列出。

关键里程碑：
- **第 56 周末**：运动模型完成
- **第 64 周末**：CCD 核心完成
- **第 70 周末**：I/O 移除完成
- **第 78 周末**：测试和优化完成
- **第 81 周末**：项目交付

---

## ⚠️ 风险缓解策略

### 风险 1：数学库性能不足
- **缓解**：提前性能测试，考虑优化关键函数
- **应急**：使用查找表（LUT）加速 sin/cos

### 风险 2：泰勒模型精度问题
- **缓解**：与用户态版本对比验证
- **应急**：降级到一阶或零阶模型

### 风险 3：Eigen 编译问题
- **缓解**：提前测试，逐步集成
- **应急**：手写替代部分 Eigen 功能

### 风险 4：内存泄漏
- **缓解**：持续 Driver Verifier 检查
- **工具**：内存追踪系统

### 风险 5：性能不达标
- **缓解**：分阶段性能测试
- **应急**：优化热点路径，使用内存池

---

## 📊 进度跟踪和报告

### 周报格式
```markdown
## 第 X 周进度报告

### 完成的任务
- [x] 任务 A
- [x] 任务 B

### 遇到的问题
- 问题 1：描述
  - 影响：高/中/低
  - 解决方案：...

### 下周计划
- [ ] 任务 C
- [ ] 任务 D

### 风险和警示
- 风险 1：...
```

### 关键指标
- **代码完成率**：已移植代码 / 总代码
- **测试覆盖率**：通过的测试 / 总测试
- **内存健康度**：无泄漏天数
- **性能达标率**：满足性能要求的模块数

---

## 🚀 开始行动

### 第 1 周行动清单

**周一**：
- [ ] 团队会议：项目启动，角色分配
- [ ] 安装 WDK 和 Musa.Runtime
- [ ] 创建 Git 仓库

**周二-周三**：
- [ ] 创建 PoC-01-MusaRuntime 项目
- [ ] 实现 STL 测试用例

**周四**：
- [ ] 运行 PoC-01，修复问题

**周五**：
- [ ] 周报，决定下周重点

---

## 📞 支持和咨询

如果遇到以下情况，立即寻求帮助：
- ❌ 关键技术验证失败（如数学库）
- ❌ 进度严重滞后（> 2 周）
- ❌ 发现新的阻断性问题
- ❌ 性能指标无法满足

---

**准备好开始了吗？让我知道你想先做什么：**

1. **启动阶段 0**：创建 PoC-01-MusaRuntime
2. **深入某个模块**：例如数学库或区间算术
3. **设置开发环境**：WDK、虚拟机等
4. **其他**：你的问题或建议

祝你好运！这将是一个艰难但有意义的旅程。🚀

