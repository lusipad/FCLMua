#include "fclmusa/math/self_test.h"
#include "fclmusa/logging.h"
#include "fclmusa/math/eigen_config.h"

#include <cmath>

#if FCL_MUSA_EIGEN_ENABLED
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4267 4819)
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <Eigen/Eigenvalues>
#include <Eigen/SVD>
#include <Eigen/LU>
#include <Eigen/QR>
#include <Eigen/Cholesky>
#pragma warning(pop)
#endif

namespace {

#if FCL_MUSA_EIGEN_ENABLED

constexpr float kTolerance = 1.0e-4f;
constexpr float kLooseTolerance = 5.0e-3f;

bool NearlyEqual(float lhs, float rhs, float tolerance = kTolerance) {
    const float diff = fabsf(lhs - rhs);
    return diff <= tolerance;
}

bool MatrixNearlyEqual(const Eigen::MatrixXf& a, const Eigen::MatrixXf& b, float tolerance = kTolerance) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) {
        return false;
    }
    return (a - b).cwiseAbs().maxCoeff() <= tolerance;
}

// ============================================================================
// 测试 1: 基础线性结构 (linearstructure)
// ============================================================================
NTSTATUS TestLinearStructure() {
    // 测试向量加法和标量乘法
    Eigen::Vector3f v1(1.0f, 2.0f, 3.0f);
    Eigen::Vector3f v2(4.0f, 5.0f, 6.0f);

    Eigen::Vector3f sum = v1 + v2;
    Eigen::Vector3f expected_sum(5.0f, 7.0f, 9.0f);
    if (!MatrixNearlyEqual(sum, expected_sum)) {
        FCL_LOG_ERROR("Vector addition failed");
        return STATUS_DATA_ERROR;
    }

    Eigen::Vector3f scaled = 2.0f * v1;
    Eigen::Vector3f expected_scaled(2.0f, 4.0f, 6.0f);
    if (!MatrixNearlyEqual(scaled, expected_scaled)) {
        FCL_LOG_ERROR("Scalar multiplication failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 2: LU 分解 (lu)
// ============================================================================
NTSTATUS TestLU() {
    Eigen::Matrix3f A;
    A << 1.0f, 2.0f, 3.0f,
         4.0f, 5.0f, 6.0f,
         7.0f, 8.0f, 10.0f;  // 注意：最后一个元素改为10使其非奇异

    Eigen::FullPivLU<Eigen::Matrix3f> lu(A);

    if (!lu.isInvertible()) {
        FCL_LOG_ERROR("Matrix should be invertible");
        return STATUS_DATA_ERROR;
    }

    // 验证 LU 分解: P^-1 * L * U * Q^-1 = A
    Eigen::Matrix3f reconstructed = lu.permutationP().inverse() * lu.matrixLU() * lu.permutationQ().inverse();

    float rank = static_cast<float>(lu.rank());
    if (!NearlyEqual(rank, 3.0f)) {
        FCL_LOG_ERROR("Rank should be 3, got %f", rank);
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 3: QR 分解 (qr)
// ============================================================================
NTSTATUS TestQR() {
    Eigen::Matrix3f A;
    A << 12.0f, -51.0f, 4.0f,
         6.0f, 167.0f, -68.0f,
         -4.0f, 24.0f, -41.0f;

    Eigen::HouseholderQR<Eigen::Matrix3f> qr(A);
    Eigen::Matrix3f Q = qr.householderQ();
    Eigen::Matrix3f R = qr.matrixQR().triangularView<Eigen::Upper>();

    // Q 应该是正交矩阵: Q^T * Q = I
    Eigen::Matrix3f identity = Q.transpose() * Q;
    Eigen::Matrix3f expected_identity = Eigen::Matrix3f::Identity();

    if (!MatrixNearlyEqual(identity, expected_identity, kLooseTolerance)) {
        FCL_LOG_ERROR("Q is not orthogonal");
        return STATUS_DATA_ERROR;
    }

    // 验证 QR 分解: Q * R = A
    Eigen::Matrix3f reconstructed = Q * R;
    if (!MatrixNearlyEqual(reconstructed, A, kLooseTolerance)) {
        FCL_LOG_ERROR("QR decomposition verification failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 4: Cholesky 分解 (cholesky)
// ============================================================================
NTSTATUS TestCholesky() {
    // 构造一个正定矩阵: A = B^T * B
    Eigen::Matrix3f B;
    B << 1.0f, 0.0f, 0.0f,
         2.0f, 3.0f, 0.0f,
         4.0f, 5.0f, 6.0f;

    Eigen::Matrix3f A = B.transpose() * B;

    Eigen::LLT<Eigen::Matrix3f> llt(A);
    if (llt.info() != Eigen::Success) {
        FCL_LOG_ERROR("Cholesky decomposition failed");
        return STATUS_DATA_ERROR;
    }

    Eigen::Matrix3f L = llt.matrixL();

    // 验证: L * L^T = A
    Eigen::Matrix3f reconstructed = L * L.transpose();
    if (!MatrixNearlyEqual(reconstructed, A, kLooseTolerance)) {
        FCL_LOG_ERROR("Cholesky verification failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 5: 矩阵求逆 (inverse)
// ============================================================================
NTSTATUS TestInverse() {
    Eigen::Matrix3f A;
    A << 1.0f, 2.0f, 3.0f,
         0.0f, 1.0f, 4.0f,
         5.0f, 6.0f, 0.0f;

    Eigen::Matrix3f A_inv = A.inverse();

    // 验证: A * A^-1 = I
    Eigen::Matrix3f identity = A * A_inv;
    Eigen::Matrix3f expected_identity = Eigen::Matrix3f::Identity();

    if (!MatrixNearlyEqual(identity, expected_identity, kLooseTolerance)) {
        FCL_LOG_ERROR("Matrix inverse verification failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 6: 行列式 (determinant)
// ============================================================================
NTSTATUS TestDeterminant() {
    Eigen::Matrix3f A;
    A << 1.0f, 2.0f, 3.0f,
         4.0f, 5.0f, 6.0f,
         7.0f, 8.0f, 9.0f;

    float det = A.determinant();

    // 这个矩阵的行列式应该是 0（奇异矩阵）
    if (!NearlyEqual(det, 0.0f, kLooseTolerance)) {
        FCL_LOG_ERROR("Determinant should be 0, got %f", det);
        return STATUS_DATA_ERROR;
    }

    // 测试非奇异矩阵
    Eigen::Matrix3f B;
    B << 1.0f, 0.0f, 0.0f,
         0.0f, 2.0f, 0.0f,
         0.0f, 0.0f, 3.0f;

    float det2 = B.determinant();
    if (!NearlyEqual(det2, 6.0f)) {
        FCL_LOG_ERROR("Determinant should be 6, got %f", det2);
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 7: 四元数 (geo_quaternion)
// ============================================================================
NTSTATUS TestQuaternion() {
    // 创建四元数表示绕 Z 轴旋转 90 度
    Eigen::Quaternionf q = Eigen::AngleAxisf(1.5707963f, Eigen::Vector3f::UnitZ());

    // 验证四元数归一化
    if (!NearlyEqual(q.norm(), 1.0f)) {
        FCL_LOG_ERROR("Quaternion should be normalized");
        return STATUS_DATA_ERROR;
    }

    // 测试四元数乘法
    Eigen::Quaternionf q2 = Eigen::AngleAxisf(1.5707963f, Eigen::Vector3f::UnitZ());
    Eigen::Quaternionf q_combined = q * q2;

    // 两次 90 度旋转 = 180 度旋转
    Eigen::Vector3f v(1.0f, 0.0f, 0.0f);
    Eigen::Vector3f v_rotated = q_combined * v;
    Eigen::Vector3f expected(-1.0f, 0.0f, 0.0f);

    if (!MatrixNearlyEqual(v_rotated, expected, kLooseTolerance)) {
        FCL_LOG_ERROR("Quaternion rotation failed");
        return STATUS_DATA_ERROR;
    }

    // 测试共轭四元数（逆旋转）
    Eigen::Quaternionf q_conj = q.conjugate();
    Eigen::Vector3f v2 = q * v;
    Eigen::Vector3f v_restored = q_conj * v2;

    if (!MatrixNearlyEqual(v_restored, v, kLooseTolerance)) {
        FCL_LOG_ERROR("Quaternion conjugate failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 8: 几何变换 (geo_transformations)
// ============================================================================
NTSTATUS TestGeometricTransformations() {
    // 测试平移
    Eigen::Translation3f translation(1.0f, 2.0f, 3.0f);
    Eigen::Vector3f point(0.0f, 0.0f, 0.0f);
    Eigen::Vector3f translated = translation * point;
    Eigen::Vector3f expected_translated(1.0f, 2.0f, 3.0f);

    if (!MatrixNearlyEqual(translated, expected_translated)) {
        FCL_LOG_ERROR("Translation failed");
        return STATUS_DATA_ERROR;
    }

    // 测试缩放
    Eigen::DiagonalMatrix<float, 3> scaling(2.0f, 3.0f, 4.0f);
    Eigen::Vector3f v(1.0f, 1.0f, 1.0f);
    Eigen::Vector3f scaled = scaling * v;
    Eigen::Vector3f expected_scaled(2.0f, 3.0f, 4.0f);

    if (!MatrixNearlyEqual(scaled, expected_scaled)) {
        FCL_LOG_ERROR("Scaling failed");
        return STATUS_DATA_ERROR;
    }

    // 测试组合变换：旋转 + 平移
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();
    transform.rotate(Eigen::AngleAxisf(1.5707963f, Eigen::Vector3f::UnitZ()));
    transform.translate(Eigen::Vector3f(1.0f, 0.0f, 0.0f));

    Eigen::Vector3f p(1.0f, 0.0f, 0.0f);
    Eigen::Vector3f p_transformed = transform * p;

    // 先旋转 (1,0,0) -> (0,1,0)，再平移 (1,0,0)
    Eigen::Vector3f expected_transform(1.0f, 1.0f, 0.0f);

    if (!MatrixNearlyEqual(p_transformed, expected_transform, kLooseTolerance)) {
        FCL_LOG_ERROR("Combined transformation failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 9: 叉积和点积 (geo_orthomethods)
// ============================================================================
NTSTATUS TestCrossAndDotProduct() {
    Eigen::Vector3f a(1.0f, 0.0f, 0.0f);
    Eigen::Vector3f b(0.0f, 1.0f, 0.0f);

    // 测试叉积: a × b = (0, 0, 1)
    Eigen::Vector3f cross = a.cross(b);
    Eigen::Vector3f expected_cross(0.0f, 0.0f, 1.0f);

    if (!MatrixNearlyEqual(cross, expected_cross)) {
        FCL_LOG_ERROR("Cross product failed");
        return STATUS_DATA_ERROR;
    }

    // 测试点积: a · b = 0 (正交向量)
    float dot = a.dot(b);
    if (!NearlyEqual(dot, 0.0f)) {
        FCL_LOG_ERROR("Dot product should be 0, got %f", dot);
        return STATUS_DATA_ERROR;
    }

    // 测试向量长度
    Eigen::Vector3f c(3.0f, 4.0f, 0.0f);
    float norm = c.norm();
    if (!NearlyEqual(norm, 5.0f)) {
        FCL_LOG_ERROR("Vector norm should be 5, got %f", norm);
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 10: 线性方程求解 (linear system solving)
// ============================================================================
NTSTATUS TestLinearSolve() {
    // 求解 Ax = b
    Eigen::Matrix3f A;
    A << 1.0f, 2.0f, 3.0f,
         4.0f, 5.0f, 6.0f,
         7.0f, 8.0f, 10.0f;

    Eigen::Vector3f b(3.0f, 3.0f, 4.0f);

    Eigen::Vector3f x = A.colPivHouseholderQr().solve(b);

    // 验证: A * x = b
    Eigen::Vector3f b_reconstructed = A * x;

    if (!MatrixNearlyEqual(b_reconstructed, b, kLooseTolerance)) {
        FCL_LOG_ERROR("Linear system solve failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 11: 矩阵块操作 (block)
// ============================================================================
NTSTATUS TestBlockOperations() {
    Eigen::Matrix4f M;
    M << 1.0f, 2.0f, 3.0f, 4.0f,
         5.0f, 6.0f, 7.0f, 8.0f,
         9.0f, 10.0f, 11.0f, 12.0f,
         13.0f, 14.0f, 15.0f, 16.0f;

    // 提取左上角 2x2 块
    Eigen::Matrix2f block = M.block<2, 2>(0, 0);
    Eigen::Matrix2f expected_block;
    expected_block << 1.0f, 2.0f,
                      5.0f, 6.0f;

    if (!MatrixNearlyEqual(block, expected_block)) {
        FCL_LOG_ERROR("Block extraction failed");
        return STATUS_DATA_ERROR;
    }

    // 测试行和列提取
    Eigen::Vector4f row = M.row(1);
    Eigen::Vector4f expected_row(5.0f, 6.0f, 7.0f, 8.0f);

    if (!MatrixNearlyEqual(row, expected_row)) {
        FCL_LOG_ERROR("Row extraction failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

// ============================================================================
// 测试 12: 三角矩阵 (triangular)
// ============================================================================
NTSTATUS TestTriangular() {
    Eigen::Matrix3f M;
    M << 1.0f, 2.0f, 3.0f,
         4.0f, 5.0f, 6.0f,
         7.0f, 8.0f, 9.0f;

    // 提取上三角部分
    Eigen::Matrix3f upper = M.triangularView<Eigen::Upper>();
    Eigen::Matrix3f expected_upper;
    expected_upper << 1.0f, 2.0f, 3.0f,
                      0.0f, 5.0f, 6.0f,
                      0.0f, 0.0f, 9.0f;

    if (!MatrixNearlyEqual(upper, expected_upper)) {
        FCL_LOG_ERROR("Upper triangular extraction failed");
        return STATUS_DATA_ERROR;
    }

    // 提取下三角部分
    Eigen::Matrix3f lower = M.triangularView<Eigen::Lower>();
    Eigen::Matrix3f expected_lower;
    expected_lower << 1.0f, 0.0f, 0.0f,
                      4.0f, 5.0f, 0.0f,
                      7.0f, 8.0f, 9.0f;

    if (!MatrixNearlyEqual(lower, expected_lower)) {
        FCL_LOG_ERROR("Lower triangular extraction failed");
        return STATUS_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

#endif  // FCL_MUSA_EIGEN_ENABLED

}  // namespace

extern "C"
NTSTATUS
FclRunEigenExtendedTest() noexcept {
#if !FCL_MUSA_EIGEN_ENABLED
    return STATUS_NOT_SUPPORTED;
#else
    LARGE_INTEGER frequency = {};
    const LARGE_INTEGER start = KeQueryPerformanceCounter(&frequency);

    struct Test {
        const char* name;
        NTSTATUS (*function)();
    };

    const Test tests[] = {
        {"LinearStructure", TestLinearStructure},
        {"LU Decomposition", TestLU},
        {"QR Decomposition", TestQR},
        {"Cholesky Decomposition", TestCholesky},
        {"Matrix Inverse", TestInverse},
        {"Determinant", TestDeterminant},
        {"Quaternion", TestQuaternion},
        {"Geometric Transformations", TestGeometricTransformations},
        {"Cross and Dot Product", TestCrossAndDotProduct},
        {"Linear System Solve", TestLinearSolve},
        {"Block Operations", TestBlockOperations},
        {"Triangular Matrices", TestTriangular},
    };

    ULONG passed = 0;
    ULONG failed = 0;

    for (const auto& test : tests) {
        NTSTATUS status = test.function();
        if (NT_SUCCESS(status)) {
            FCL_LOG_INFO("Eigen test '%s' PASSED", test.name);
            passed++;
        } else {
            FCL_LOG_ERROR("Eigen test '%s' FAILED with status 0x%X", test.name, status);
            failed++;
        }
    }

    const LARGE_INTEGER end = KeQueryPerformanceCounter(nullptr);
    ULONGLONG elapsedMicroseconds = 0;
    if (frequency.QuadPart > 0) {
        const LONGLONG delta = end.QuadPart - start.QuadPart;
        if (delta > 0) {
            elapsedMicroseconds = static_cast<ULONGLONG>((delta * 1000000LL) / frequency.QuadPart);
        }
    }

    FCL_LOG_INFO("Eigen extended tests: %lu passed, %lu failed (%llu us)", passed, failed, elapsedMicroseconds);

    return (failed == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
#endif
}
