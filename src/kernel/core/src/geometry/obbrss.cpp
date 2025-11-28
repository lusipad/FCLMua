#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fclmusa/geometry/obbrss.h"

#include <algorithm>
#include <array>
#include <vector>

#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/math/eigen_config.h"

#if FCL_MUSA_EIGEN_ENABLED
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4267 4819)
#include <Eigen/Eigenvalues>
#pragma warning(pop)
#endif

using namespace fclmusa::geom;

namespace {

constexpr float kProjectionTolerance = 1e-5f;

void InitializeIdentityAxes(FCL_OBBRSS* volume) noexcept {
    volume->Axis[0] = {1.0f, 0.0f, 0.0f};
    volume->Axis[1] = {0.0f, 1.0f, 0.0f};
    volume->Axis[2] = {0.0f, 0.0f, 1.0f};
}

FCL_OBBRSS CreateEmptyVolume() noexcept {
    FCL_OBBRSS result = {};
    InitializeIdentityAxes(&result);
    return result;
}

void ComputeAabb(
    const FCL_VECTOR3* points,
    size_t count,
    FCL_VECTOR3* outMin,
    FCL_VECTOR3* outMax) noexcept {
    FCL_VECTOR3 minPoint = points[0];
    FCL_VECTOR3 maxPoint = points[0];
    for (size_t i = 1; i < count; ++i) {
        minPoint.X = std::min(minPoint.X, points[i].X);
        minPoint.Y = std::min(minPoint.Y, points[i].Y);
        minPoint.Z = std::min(minPoint.Z, points[i].Z);
        maxPoint.X = std::max(maxPoint.X, points[i].X);
        maxPoint.Y = std::max(maxPoint.Y, points[i].Y);
        maxPoint.Z = std::max(maxPoint.Z, points[i].Z);
    }
    *outMin = minPoint;
    *outMax = maxPoint;
}

#if FCL_MUSA_EIGEN_ENABLED
void SortEigenVectors(Eigen::Matrix3f* eigenvectors, Eigen::Vector3f* eigenvalues) {
    std::array<int, 3> order = {0, 1, 2};
    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        return (*eigenvalues)[lhs] > (*eigenvalues)[rhs];
    });
    Eigen::Matrix3f sortedVectors;
    Eigen::Vector3f sortedValues;
    for (int i = 0; i < 3; ++i) {
        sortedVectors.col(i) = eigenvectors->col(order[i]);
        sortedValues[i] = (*eigenvalues)[order[i]];
    }
    *eigenvectors = sortedVectors;
    *eigenvalues = sortedValues;
}
#endif

FCL_OBBRSS BuildAlignedVolume(
    const FCL_VECTOR3* points,
    size_t count) noexcept {
    FCL_VECTOR3 minPoint = {};
    FCL_VECTOR3 maxPoint = {};
    ComputeAabb(points, count, &minPoint, &maxPoint);

    FCL_OBBRSS volume = {};
    InitializeIdentityAxes(&volume);
    volume.Center = Scale(Add(minPoint, maxPoint), 0.5f);
    volume.Extents = Scale(Subtract(maxPoint, minPoint), 0.5f);
    volume.Radius = Length(volume.Extents);
    return volume;
}

#if FCL_MUSA_EIGEN_ENABLED
FCL_OBBRSS BuildPcaVolume(
    const FCL_VECTOR3* points,
    size_t count) noexcept {
    FCL_VECTOR3 mean = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < count; ++i) {
        mean = Add(mean, points[i]);
    }
    mean = Scale(mean, 1.0f / static_cast<float>(count));

    Eigen::Matrix3f covariance = Eigen::Matrix3f::Zero();
    for (size_t i = 0; i < count; ++i) {
        Eigen::Vector3f centered(points[i].X - mean.X, points[i].Y - mean.Y, points[i].Z - mean.Z);
        covariance += centered * centered.transpose();
    }
    covariance /= static_cast<float>(count);

    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver(covariance);
    if (solver.info() != Eigen::Success) {
        return BuildAlignedVolume(points, count);
    }

    Eigen::Matrix3f eigenvectors = solver.eigenvectors();
    Eigen::Vector3f eigenvalues = solver.eigenvalues();
    SortEigenVectors(&eigenvectors, &eigenvalues);

    float minProj[3] = {};
    float maxProj[3] = {};
    for (int axis = 0; axis < 3; ++axis) {
        minProj[axis] = FLT_MAX;
        maxProj[axis] = -FLT_MAX;
    }

    for (size_t i = 0; i < count; ++i) {
        Eigen::Vector3f centered(points[i].X - mean.X, points[i].Y - mean.Y, points[i].Z - mean.Z);
        for (int axis = 0; axis < 3; ++axis) {
            const float projection = centered.dot(eigenvectors.col(axis));
            minProj[axis] = std::min(minProj[axis], projection);
            maxProj[axis] = std::max(maxProj[axis], projection);
        }
    }

    FCL_OBBRSS volume = {};
    for (int axis = 0; axis < 3; ++axis) {
        volume.Axis[axis].X = eigenvectors(0, axis);
        volume.Axis[axis].Y = eigenvectors(1, axis);
        volume.Axis[axis].Z = eigenvectors(2, axis);
        volume.Extents.X = 0.0f;
        volume.Extents.Y = 0.0f;
        volume.Extents.Z = 0.0f;
    }

    volume.Center = mean;
    for (int axis = 0; axis < 3; ++axis) {
        const float mid = (minProj[axis] + maxProj[axis]) * 0.5f;
        volume.Center = Add(volume.Center, Scale(volume.Axis[axis], mid));
        (&volume.Extents.X)[axis] = (maxProj[axis] - minProj[axis]) * 0.5f;
    }

    volume.Radius = (&volume.Extents.X)[2];
    return volume;
}
#endif

FCL_VECTOR3 GetCorner(
    const FCL_OBBRSS& volume,
    float sx,
    float sy,
    float sz) noexcept {
    FCL_VECTOR3 corner = volume.Center;
    corner = Add(corner, Scale(volume.Axis[0], sx * volume.Extents.X));
    corner = Add(corner, Scale(volume.Axis[1], sy * volume.Extents.Y));
    corner = Add(corner, Scale(volume.Axis[2], sz * volume.Extents.Z));
    return corner;
}

}  // namespace

extern "C"
FCL_OBBRSS
FclObbrssFromPoints(
    _In_reads_(pointCount) const FCL_VECTOR3* points,
    _In_ size_t pointCount) noexcept {
    if (points == nullptr || pointCount == 0) {
        return CreateEmptyVolume();
    }

#if FCL_MUSA_EIGEN_ENABLED
    return BuildPcaVolume(points, pointCount);
#else
    return BuildAlignedVolume(points, pointCount);
#endif
}

extern "C"
FCL_OBBRSS
FclObbrssMerge(
    _In_ const FCL_OBBRSS* lhs,
    _In_ const FCL_OBBRSS* rhs) noexcept {
    if (lhs == nullptr) {
        return (rhs != nullptr) ? *rhs : CreateEmptyVolume();
    }
    if (rhs == nullptr) {
        return *lhs;
    }

    std::vector<FCL_VECTOR3> points;
    points.reserve(16);

    const std::array<float, 2> signs = {-1.0f, 1.0f};
    for (float sx : signs) {
        for (float sy : signs) {
            for (float sz : signs) {
                points.push_back(GetCorner(*lhs, sx, sy, sz));
                points.push_back(GetCorner(*rhs, sx, sy, sz));
            }
        }
    }

    return FclObbrssFromPoints(points.data(), points.size());
}

extern "C"
BOOLEAN
FclObbrssOverlap(
    _In_ const FCL_OBBRSS* lhs,
    _In_ const FCL_OBBRSS* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return FALSE;
    }

    float R[3][3];
    float AbsR[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = Dot(lhs->Axis[i], rhs->Axis[j]);
            AbsR[i][j] = fabs(R[i][j]) + kAxisEpsilon;
        }
    }

    const FCL_VECTOR3 translation = Subtract(rhs->Center, lhs->Center);
    const float t[3] = {
        Dot(translation, lhs->Axis[0]),
        Dot(translation, lhs->Axis[1]),
        Dot(translation, lhs->Axis[2])};

    auto sat = [&](float projection, float radius) -> bool {
        return projection > radius + kProjectionTolerance;
    };

    for (int i = 0; i < 3; ++i) {
        const float ra = (&lhs->Extents.X)[i];
        float rb = 0.0f;
        for (int j = 0; j < 3; ++j) {
            rb += (&rhs->Extents.X)[j] * AbsR[i][j];
        }
        if (sat(fabs(t[i]), ra + rb)) {
            return FALSE;
        }
    }

    for (int j = 0; j < 3; ++j) {
        float ra = 0.0f;
        for (int i = 0; i < 3; ++i) {
            ra += (&lhs->Extents.X)[i] * AbsR[i][j];
        }
        const float rb = (&rhs->Extents.X)[j];
        const float projection =
            fabs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]);
        if (sat(projection, ra + rb)) {
            return FALSE;
        }
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            FCL_VECTOR3 axis = Cross(lhs->Axis[i], rhs->Axis[j]);
            if (Length(axis) <= kAxisEpsilon) {
                continue;
            }
            const float ra =
                (&lhs->Extents.X)[(i + 1) % 3] * AbsR[(i + 2) % 3][j] +
                (&lhs->Extents.X)[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
            const float rb =
                (&rhs->Extents.X)[(j + 1) % 3] * AbsR[i][(j + 2) % 3] +
                (&rhs->Extents.X)[(j + 2) % 3] * AbsR[i][(j + 1) % 3];
            const float proj =
                fabs(t[(i + 1) % 3] * R[(i + 2) % 3][j] -
                     t[(i + 2) % 3] * R[(i + 1) % 3][j]);
            if (sat(proj, ra + rb)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}
