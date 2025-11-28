#include "fclmusa/math/self_test.h"

#include "fclmusa/logging.h"
#include "fclmusa/math/eigen_config.h"

#include <cmath>

#if FCL_MUSA_EIGEN_ENABLED
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4267 4819)
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <Eigen/SVD>
#pragma warning(pop)
#endif

namespace {

#if FCL_MUSA_EIGEN_ENABLED
constexpr float kTolerance = 1.0e-4f;
constexpr float kLooseTolerance = 5.0e-3f;

bool NearlyEqual(float lhs, float rhs) {
    const float diff = fabsf(lhs - rhs);
    return diff <= kTolerance;
}
#endif

}  // namespace

extern "C"
NTSTATUS
FclRunEigenSmokeTest() noexcept {
#if !FCL_MUSA_EIGEN_ENABLED
    return STATUS_NOT_SUPPORTED;
#else
    LARGE_INTEGER frequency = {};
    const LARGE_INTEGER start = KeQueryPerformanceCounter(&frequency);

    Eigen::Matrix3f rotation = Eigen::AngleAxisf(0.5f, Eigen::Vector3f::UnitX()).toRotationMatrix();
    if (!NearlyEqual(rotation.determinant(), 1.0f)) {
        return STATUS_DATA_NOT_ACCEPTED;
    }

    Eigen::Vector3f vector(1.0f, 2.0f, -3.0f);
    const Eigen::Vector3f transformed = rotation * vector;
    const Eigen::Vector3f restored = rotation.transpose() * transformed;

    if ((vector - restored).norm() > kTolerance) {
        return STATUS_DATA_NOT_ACCEPTED;
    }

    Eigen::Matrix3f a = Eigen::Matrix3f::Random();
    Eigen::Matrix3f b = Eigen::Matrix3f::Random();
    const Eigen::Matrix3f c = a * b;
    if (!NearlyEqual(static_cast<float>((c - (a * b)).norm()), 0.0f)) {
        return STATUS_DATA_NOT_ACCEPTED;
    }

    Eigen::Matrix2f symmetric;
    symmetric << 3.0f, 2.0f,
                 2.0f, 3.0f;
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix2f> eigenSolver(symmetric);
    if (eigenSolver.info() != Eigen::Success) {
        return STATUS_DATA_NOT_ACCEPTED;
    }
    const Eigen::Vector2f eigenvalues = eigenSolver.eigenvalues();
    if (!NearlyEqual(eigenvalues[0], 1.0f) || !NearlyEqual(eigenvalues[1], 5.0f)) {
        return STATUS_DATA_NOT_ACCEPTED;
    }
    const Eigen::Matrix2f reconstructed = eigenSolver.eigenvectors() * eigenvalues.asDiagonal() * eigenSolver.eigenvectors().transpose();
    if ((reconstructed - symmetric).cwiseAbs().maxCoeff() > kLooseTolerance) {
        return STATUS_DATA_NOT_ACCEPTED;
    }

    Eigen::Matrix3f decomposed = Eigen::Matrix3f::Random();
    Eigen::JacobiSVD<Eigen::Matrix3f> svd(decomposed, Eigen::ComputeFullU | Eigen::ComputeFullV);
    if (svd.info() != Eigen::Success) {
        return STATUS_DATA_NOT_ACCEPTED;
    }
    const Eigen::Matrix3f svdProduct = svd.matrixU() * svd.singularValues().asDiagonal() * svd.matrixV().transpose();
    if ((svdProduct - decomposed).norm() > kLooseTolerance) {
        return STATUS_DATA_NOT_ACCEPTED;
    }

    const LARGE_INTEGER end = KeQueryPerformanceCounter(nullptr);
    ULONGLONG elapsedMicroseconds = 0;
    if (frequency.QuadPart > 0) {
        const LONGLONG delta = end.QuadPart - start.QuadPart;
        if (delta > 0) {
            elapsedMicroseconds = static_cast<ULONGLONG>((delta * 1000000LL) / frequency.QuadPart);
        }
    }

    FCL_LOG_INFO("Eigen smoke test passed (%llu us)", elapsedMicroseconds);
    return STATUS_SUCCESS;
#endif
}

