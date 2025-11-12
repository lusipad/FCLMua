#pragma once

#include <ntddk.h>

#include <cmath>
#include <float.h>

#include "fclmusa/geometry.h"

namespace fclmusa::geom {

inline constexpr float kLinearTolerance = 1e-5f;
inline constexpr float kSingularityEpsilon = 1e-6f;
inline constexpr float kAxisEpsilon = 1e-6f;

inline bool IsFiniteFloat(float value) noexcept {
    return (value == value) && (value < FLT_MAX) && (value > -FLT_MAX);
}

inline bool IsValidVector(const FCL_VECTOR3& value) noexcept {
    return IsFiniteFloat(value.X) && IsFiniteFloat(value.Y) && IsFiniteFloat(value.Z);
}

inline bool IsValidMatrix(const FCL_MATRIX3X3& matrix) noexcept {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (!IsFiniteFloat(matrix.M[row][col])) {
                return false;
            }
        }
    }
    return true;
}

inline bool IsValidTransform(const FCL_TRANSFORM& transform) noexcept {
    return IsValidMatrix(transform.Rotation) && IsValidVector(transform.Translation);
}

inline FCL_TRANSFORM IdentityTransform() noexcept {
    FCL_TRANSFORM transform = {};
    transform.Rotation.M[0][0] = 1.0f;
    transform.Rotation.M[1][1] = 1.0f;
    transform.Rotation.M[2][2] = 1.0f;
    transform.Translation = {0.0f, 0.0f, 0.0f};
    return transform;
}

inline FCL_VECTOR3 Add(const FCL_VECTOR3& lhs, const FCL_VECTOR3& rhs) noexcept {
    return {lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z};
}

inline FCL_VECTOR3 Subtract(const FCL_VECTOR3& lhs, const FCL_VECTOR3& rhs) noexcept {
    return {lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z};
}

inline FCL_VECTOR3 Scale(const FCL_VECTOR3& value, float scalar) noexcept {
    return {value.X * scalar, value.Y * scalar, value.Z * scalar};
}

inline float Dot(const FCL_VECTOR3& lhs, const FCL_VECTOR3& rhs) noexcept {
    return lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z;
}

inline float Length(const FCL_VECTOR3& value) noexcept {
    return static_cast<float>(sqrt(Dot(value, value)));
}

inline FCL_VECTOR3 Normalize(const FCL_VECTOR3& value) noexcept {
    const float len = Length(value);
    if (len <= kSingularityEpsilon) {
        return {1.0f, 0.0f, 0.0f};
    }
    return Scale(value, 1.0f / len);
}

inline FCL_VECTOR3 Cross(const FCL_VECTOR3& lhs, const FCL_VECTOR3& rhs) noexcept {
    return {
        lhs.Y * rhs.Z - lhs.Z * rhs.Y,
        lhs.Z * rhs.X - lhs.X * rhs.Z,
        lhs.X * rhs.Y - lhs.Y * rhs.X};
}

inline FCL_MATRIX3X3 MultiplyMatrix(const FCL_MATRIX3X3& lhs, const FCL_MATRIX3X3& rhs) noexcept {
    FCL_MATRIX3X3 result = {};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            result.M[row][col] = lhs.M[row][0] * rhs.M[0][col] +
                                 lhs.M[row][1] * rhs.M[1][col] +
                                 lhs.M[row][2] * rhs.M[2][col];
        }
    }
    return result;
}

inline FCL_VECTOR3 MatrixVectorMultiply(const FCL_MATRIX3X3& matrix, const FCL_VECTOR3& value) noexcept {
    return {
        matrix.M[0][0] * value.X + matrix.M[0][1] * value.Y + matrix.M[0][2] * value.Z,
        matrix.M[1][0] * value.X + matrix.M[1][1] * value.Y + matrix.M[1][2] * value.Z,
        matrix.M[2][0] * value.X + matrix.M[2][1] * value.Y + matrix.M[2][2] * value.Z};
}

inline FCL_VECTOR3 TransformPoint(const FCL_TRANSFORM& transform, const FCL_VECTOR3& point) noexcept {
    return Add(MatrixVectorMultiply(transform.Rotation, point), transform.Translation);
}

inline FCL_MATRIX3X3 TransposeMatrix(const FCL_MATRIX3X3& matrix) noexcept {
    FCL_MATRIX3X3 result = {};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            result.M[row][col] = matrix.M[col][row];
        }
    }
    return result;
}

inline float Clamp(float value, float minValue, float maxValue) noexcept {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

inline FCL_VECTOR3 LerpVector(const FCL_VECTOR3& a, const FCL_VECTOR3& b, double t) noexcept {
    const float alpha = static_cast<float>(Clamp(static_cast<float>(t), 0.0f, 1.0f));
    return {
        a.X + (b.X - a.X) * alpha,
        a.Y + (b.Y - a.Y) * alpha,
        a.Z + (b.Z - a.Z) * alpha};
}

typedef struct _FCL_QUATERNION {
    float W;
    float X;
    float Y;
    float Z;
} FCL_QUATERNION, *PFCL_QUATERNION;

inline FCL_QUATERNION QuaternionNormalize(const FCL_QUATERNION& quat) noexcept {
    const float length = static_cast<float>(sqrt(quat.W * quat.W + quat.X * quat.X + quat.Y * quat.Y + quat.Z * quat.Z));
    if (length <= kSingularityEpsilon) {
        return {1.0f, 0.0f, 0.0f, 0.0f};
    }
    const float inv = 1.0f / length;
    return {quat.W * inv, quat.X * inv, quat.Y * inv, quat.Z * inv};
}

inline FCL_QUATERNION QuaternionFromMatrix(const FCL_MATRIX3X3& matrix) noexcept {
    FCL_QUATERNION quat = {};
    const float trace = matrix.M[0][0] + matrix.M[1][1] + matrix.M[2][2];
    if (trace > 0.0f) {
        const float s = static_cast<float>(sqrt(trace + 1.0f)) * 2.0f;
        quat.W = 0.25f * s;
        quat.X = (matrix.M[2][1] - matrix.M[1][2]) / s;
        quat.Y = (matrix.M[0][2] - matrix.M[2][0]) / s;
        quat.Z = (matrix.M[1][0] - matrix.M[0][1]) / s;
    } else if (matrix.M[0][0] > matrix.M[1][1] && matrix.M[0][0] > matrix.M[2][2]) {
        const float s = static_cast<float>(sqrt(1.0f + matrix.M[0][0] - matrix.M[1][1] - matrix.M[2][2])) * 2.0f;
        quat.W = (matrix.M[2][1] - matrix.M[1][2]) / s;
        quat.X = 0.25f * s;
        quat.Y = (matrix.M[0][1] + matrix.M[1][0]) / s;
        quat.Z = (matrix.M[0][2] + matrix.M[2][0]) / s;
    } else if (matrix.M[1][1] > matrix.M[2][2]) {
        const float s = static_cast<float>(sqrt(1.0f + matrix.M[1][1] - matrix.M[0][0] - matrix.M[2][2])) * 2.0f;
        quat.W = (matrix.M[0][2] - matrix.M[2][0]) / s;
        quat.X = (matrix.M[0][1] + matrix.M[1][0]) / s;
        quat.Y = 0.25f * s;
        quat.Z = (matrix.M[1][2] + matrix.M[2][1]) / s;
    } else {
        const float s = static_cast<float>(sqrt(1.0f + matrix.M[2][2] - matrix.M[0][0] - matrix.M[1][1])) * 2.0f;
        quat.W = (matrix.M[1][0] - matrix.M[0][1]) / s;
        quat.X = (matrix.M[0][2] + matrix.M[2][0]) / s;
        quat.Y = (matrix.M[1][2] + matrix.M[2][1]) / s;
        quat.Z = 0.25f * s;
    }
    return QuaternionNormalize(quat);
}

inline FCL_MATRIX3X3 QuaternionToMatrix(const FCL_QUATERNION& quaternion) noexcept {
    const FCL_QUATERNION quat = QuaternionNormalize(quaternion);
    const float xx = quat.X * quat.X;
    const float yy = quat.Y * quat.Y;
    const float zz = quat.Z * quat.Z;
    const float xy = quat.X * quat.Y;
    const float xz = quat.X * quat.Z;
    const float yz = quat.Y * quat.Z;
    const float wx = quat.W * quat.X;
    const float wy = quat.W * quat.Y;
    const float wz = quat.W * quat.Z;

    FCL_MATRIX3X3 rotation = {};
    rotation.M[0][0] = 1.0f - 2.0f * (yy + zz);
    rotation.M[0][1] = 2.0f * (xy - wz);
    rotation.M[0][2] = 2.0f * (xz + wy);
    rotation.M[1][0] = 2.0f * (xy + wz);
    rotation.M[1][1] = 1.0f - 2.0f * (xx + zz);
    rotation.M[1][2] = 2.0f * (yz - wx);
    rotation.M[2][0] = 2.0f * (xz - wy);
    rotation.M[2][1] = 2.0f * (yz + wx);
    rotation.M[2][2] = 1.0f - 2.0f * (xx + yy);
    return rotation;
}

inline float QuaternionDot(const FCL_QUATERNION& a, const FCL_QUATERNION& b) noexcept {
    return a.W * b.W + a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

inline FCL_QUATERNION QuaternionSlerp(FCL_QUATERNION a, FCL_QUATERNION b, double t) noexcept {
    const float alpha = Clamp(static_cast<float>(t), 0.0f, 1.0f);
    float dot = QuaternionDot(a, b);
    if (dot < 0.0f) {
        dot = -dot;
        b.W = -b.W;
        b.X = -b.X;
        b.Y = -b.Y;
        b.Z = -b.Z;
    }

    const float kThreshold = 0.9995f;
    if (dot > kThreshold) {
        const FCL_QUATERNION result = {
            a.W + alpha * (b.W - a.W),
            a.X + alpha * (b.X - a.X),
            a.Y + alpha * (b.Y - a.Y),
            a.Z + alpha * (b.Z - a.Z)};
        return QuaternionNormalize(result);
    }

    const float theta0 = static_cast<float>(acos(dot));
    const float theta = theta0 * alpha;
    const float sinTheta0 = static_cast<float>(sin(theta0));
    const float sinTheta = static_cast<float>(sin(theta));

    const float s0 = static_cast<float>(cos(theta)) - dot * sinTheta / sinTheta0;
    const float s1 = sinTheta / sinTheta0;

    return {
        s0 * a.W + s1 * b.W,
        s0 * a.X + s1 * b.X,
        s0 * a.Y + s1 * b.Y,
        s0 * a.Z + s1 * b.Z};
}

inline FCL_QUATERNION QuaternionFromAxisAngle(const FCL_VECTOR3& axis, float angle) noexcept {
    const float halfAngle = angle * 0.5f;
    const float sinHalf = static_cast<float>(sin(halfAngle));
    const FCL_VECTOR3 normalized = Normalize(axis);
    return QuaternionNormalize({
        static_cast<float>(cos(halfAngle)),
        normalized.X * sinHalf,
        normalized.Y * sinHalf,
        normalized.Z * sinHalf});
}

inline FCL_QUATERNION QuaternionMultiply(const FCL_QUATERNION& lhs, const FCL_QUATERNION& rhs) noexcept {
    return QuaternionNormalize({
        lhs.W * rhs.W - lhs.X * rhs.X - lhs.Y * rhs.Y - lhs.Z * rhs.Z,
        lhs.W * rhs.X + lhs.X * rhs.W + lhs.Y * rhs.Z - lhs.Z * rhs.Y,
        lhs.W * rhs.Y - lhs.X * rhs.Z + lhs.Y * rhs.W + lhs.Z * rhs.X,
        lhs.W * rhs.Z + lhs.X * rhs.Y - lhs.Y * rhs.X + lhs.Z * rhs.W});
}

inline bool AxisAngleFromMatrix(
    const FCL_MATRIX3X3& rotation,
    FCL_VECTOR3* axis,
    float* angle) noexcept {
    if (axis == nullptr || angle == nullptr) {
        return false;
    }

    const float trace = rotation.M[0][0] + rotation.M[1][1] + rotation.M[2][2];
    float computedAngle = static_cast<float>(acos(Clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f)));

    if (computedAngle <= kSingularityEpsilon) {
        *axis = {1.0f, 0.0f, 0.0f};
        *angle = 0.0f;
        return true;
    }

    const float denom = 2.0f * static_cast<float>(sin(computedAngle));
    if (fabs(denom) <= kSingularityEpsilon) {
        return false;
    }

    *axis = Normalize({
        (rotation.M[2][1] - rotation.M[1][2]) / denom,
        (rotation.M[0][2] - rotation.M[2][0]) / denom,
        (rotation.M[1][0] - rotation.M[0][1]) / denom});
    *angle = computedAngle;
    return true;
}

inline FCL_MATRIX3X3 RotationMatrixFromAxisAngle(const FCL_VECTOR3& axis, float angle) noexcept {
    const FCL_VECTOR3 normalized = Normalize(axis);
    const float x = normalized.X;
    const float y = normalized.Y;
    const float z = normalized.Z;
    const float c = static_cast<float>(cos(angle));
    const float s = static_cast<float>(sin(angle));
    const float t = 1.0f - c;

    FCL_MATRIX3X3 rotation = {};
    rotation.M[0][0] = c + x * x * t;
    rotation.M[0][1] = x * y * t - z * s;
    rotation.M[0][2] = x * z * t + y * s;
    rotation.M[1][0] = y * x * t + z * s;
    rotation.M[1][1] = c + y * y * t;
    rotation.M[1][2] = y * z * t - x * s;
    rotation.M[2][0] = z * x * t - y * s;
    rotation.M[2][1] = z * y * t + x * s;
    rotation.M[2][2] = c + z * z * t;
    return rotation;
}

}  // namespace fclmusa::geom
