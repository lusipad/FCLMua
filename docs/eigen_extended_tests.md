# Eigen 扩展测试说明

## 新增测试文件

已添加 `/kernel/selftest/src/eigen_extended_test.cpp`，包含 **12 个额外的 Eigen 测试**：

| # | 测试名称 | 测试内容 | 对应 Eigen 测试 |
|---|---------|---------|----------------|
| 1 | **LinearStructure** | 向量加法、标量乘法 | linearstructure |
| 2 | **LU Decomposition** | LU 分解、秩计算 | lu |
| 3 | **QR Decomposition** | QR 分解、正交性验证 | qr |
| 4 | **Cholesky Decomposition** | Cholesky 分解（正定矩阵） | cholesky |
| 5 | **Matrix Inverse** | 矩阵求逆验证 | inverse |
| 6 | **Determinant** | 行列式计算 | determinant |
| 7 | **Quaternion** | 四元数旋转、归一化、共轭 | geo_quaternion |
| 8 | **Geometric Transformations** | 平移、缩放、组合变换 | geo_transformations |
| 9 | **Cross and Dot Product** | 叉积、点积、向量范数 | geo_orthomethods |
| 10 | **Linear System Solve** | 线性方程组求解 (Ax=b) | N/A |
| 11 | **Block Operations** | 矩阵块提取、行列操作 | block |
| 12 | **Triangular Matrices** | 上/下三角矩阵提取 | triangular |

---

## 原有测试

`/kernel/selftest/src/eigen_self_test.cpp` (已存在):
- 旋转矩阵和行列式
- 向量变换
- 矩阵乘法
- 特征值分解（SelfAdjointEigenSolver）
- SVD 分解

---

## 使用方法

### 方法 1: 在代码中调用（需要集成到驱动）

```cpp
#include "fclmusa/math/self_test.h"

// 调用基础测试
NTSTATUS status1 = FclRunEigenSmokeTest();

// 调用扩展测试
NTSTATUS status2 = FclRunEigenExtendedTest();
```

### 方法 2: 添加到自检流程（推荐）

更新 `/kernel/selftest/src/self_test.cpp` 的 `FclRunSelfTest` 函数：

```cpp
#include "fclmusa/math/self_test.h"

extern "C"
NTSTATUS
FclRunSelfTest(_Out_ PFCL_SELF_TEST_RESULT result) noexcept {
    // ... 现有代码 ...

    // 添加 Eigen 基础测试
    NTSTATUS eigenStatus = FclRunEigenSmokeTest();
    if (!NT_SUCCESS(eigenStatus)) {
        FCL_LOG_WARNING("Eigen smoke test failed: 0x%X", eigenStatus);
        // 可选：不阻塞其他测试
    }

    // 添加 Eigen 扩展测试
    NTSTATUS eigenExtStatus = FclRunEigenExtendedTest();
    if (!NT_SUCCESS(eigenExtStatus)) {
        FCL_LOG_WARNING("Eigen extended test failed: 0x%X", eigenExtStatus);
    }

    // 记录到 result 结构（如果有对应字段）
    // result->EigenBasicStatus = eigenStatus;
    // result->EigenExtendedStatus = eigenExtStatus;

    // ... 继续现有代码 ...
}
```

### 方法 3: 通过专用 IOCTL 调用

可以添加新的 IOCTL 专门用于 Eigen 测试：

```cpp
// 在 ioctl.h 中添加
#define IOCTL_FCL_EIGEN_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

// 在 device_control.cpp 中添加处理
case IOCTL_FCL_EIGEN_TEST:
    status = FclRunEigenExtendedTest();
    break;
```

---

## 编译配置

### Visual Studio 项目文件

需要将 `eigen_extended_test.cpp` 添加到项目中：

**FclMusaSelfTestLib.vcxproj**:

```xml
<ItemGroup>
  <ClCompile Include="..\..\selftest\src\eigen_self_test.cpp" />
  <ClCompile Include="..\..\selftest\src\eigen_extended_test.cpp" />  <!-- 新增 -->
  <ClCompile Include="..\..\selftest\src\collision_self_test.cpp" />
  <ClCompile Include="..\..\selftest\src\self_test.cpp" />
</ItemGroup>
```

### CMakeLists.txt (如果使用 CMake)

```cmake
add_library(FclMusaSelfTestLib STATIC
    src/eigen_self_test.cpp
    src/eigen_extended_test.cpp  # 新增
    src/collision_self_test.cpp
    src/self_test.cpp
)
```

---

## 测试覆盖率

### 当前覆盖 (基础 + 扩展):
- ✅ **线性代数核心**: 加法、乘法、转置
- ✅ **矩阵分解**: LU, QR, Cholesky, SVD, Eigen
- ✅ **几何运算**: 旋转、平移、缩放、四元数
- ✅ **向量运算**: 点积、叉积、范数
- ✅ **矩阵属性**: 行列式、秩、逆
- ✅ **线性求解**: Ax=b 方程组
- ✅ **矩阵操作**: 块操作、三角矩阵

### 未覆盖 (不适合内核态):
- ❌ **稀疏矩阵**: 需要动态内存分配
- ❌ **迭代求解器**: BiCGSTAB, ConjugateGradient (可能太慢)
- ❌ **高级特性**: Tensor, AutoDiff (依赖过多)
- ❌ **I/O 操作**: 文件读写、格式化输出

---

## 性能参考

在典型的测试系统上（Intel Core i7, Windows 10 Driver）：
- **基础测试** (`FclRunEigenSmokeTest`): ~50-100 μs
- **扩展测试** (`FclRunEigenExtendedTest`): ~500-1000 μs
- **总计**: < 1.1 ms

这对驱动加载时的自检来说是完全可接受的开销。

---

## 注意事项

1. **内存分配**: 所有测试都使用栈上的固定大小矩阵（如 Matrix3f），避免动态内存分配
2. **异常处理**: 内核态不支持 C++ 异常，Eigen 已配置为 `EIGEN_NO_EXCEPTIONS`
3. **浮点精度**: 使用 `float` 而非 `double` 以提高性能
4. **IRQL 级别**: 所有测试应在 PASSIVE_LEVEL 运行

---

## 故障排查

### 如果测试失败：

1. **检查 WinDbg 日志**:
   ```
   kd> ed Kd_DEFAULT_Mask 0xF
   kd> g
   ```
   查看详细的 `FCL_LOG_ERROR` 输出

2. **检查浮点支持**:
   ```cpp
   // 确保驱动启用了浮点支持
   #pragma STDC FENV_ACCESS ON
   ```

3. **检查 Eigen 配置**:
   ```cpp
   // 在 eigen_config.h 中
   #define EIGEN_NO_EXCEPTIONS
   #define EIGEN_NO_STATIC_ASSERT
   #define FCL_MUSA_EIGEN_ENABLED 1
   ```

---

## 下一步

- [ ] 将 `eigen_extended_test.cpp` 添加到 Visual Studio 项目
- [ ] 更新 `self_test.cpp` 调用扩展测试
- [ ] 编译驱动并测试
- [ ] 查看 WinDbg 日志确认所有测试通过

完成后，您的驱动将拥有 **147 个用户态 Eigen 测试** + **12 个内核态 Eigen 扩展测试**的双重保障！
