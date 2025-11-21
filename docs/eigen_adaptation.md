# Eigen 适配说明

本项目默认不直接携带 Eigen 头文件，为了在有需要时启用 Eigen，请参考下述约束：

1. **放置头文件**：将官方 Eigen 发行版（3.3+）复制到 `external/Eigen/` 目录，使得 `external/Eigen/Eigen/Core` 可被编译器找到。`fclmusa/math/eigen_config.h` 会通过 `__has_include(<Eigen/Core>)` 自动检测并启用。
2. **禁用不兼容特性**：`eigen_config.h` 会自动定义：
   - `EIGEN_NO_DEBUG` / `EIGEN_NO_STATIC_ASSERT`
   - `EIGEN_DONT_ALIGN` / `EIGEN_DONT_VECTORIZE`
   - `EIGEN_HAS_C99_MATH`
3. **内存分配**：Eigen 的 `new/delete` 会被宏重定向到 `fclmusa::memory::Allocate/Free`，确保所有分配都使用 NonPagedPool，并受池统计跟踪。
4. **自检覆盖**：`FclRunEigenSmokeTest()` 会在 PASSIVE_LEVEL 运行，除了基础矩阵乘法外，还会执行自伴矩阵特征值分解与 `JacobiSVD` 重建，并记录运行耗时，验证 Eigen 高级算子在内核模式下稳定运行。

> **注意**：未检测到 `<Eigen/Core>` 时，`FCL_MUSA_EIGEN_ENABLED` 自动为 0，构建仍可进行（与当前阶段一致）。
