#include <ntddk.h>
#include <wdm.h>

#include "fclmusa/collision.h"
#include "fclmusa/distance.h"
#include "fclmusa/geometry/math_utils.h"

namespace {

constexpr double kDefaultTolerance = 1e-4;
constexpr ULONG kDefaultIterations = 16;
constexpr double kMinRelativeSpeed = 1e-8;

double Clamp01(double value) noexcept {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

double ComputeLinearSpeed(const FCL_INTERP_MOTION& motion) noexcept {
    const FCL_VECTOR3 delta = fclmusa::geom::Subtract(motion.End.Translation, motion.Start.Translation);
    return static_cast<double>(fclmusa::geom::Length(delta));
}

double EstimateRelativeSpeed(const FCL_CONTINUOUS_COLLISION_QUERY* query) noexcept {
    if (query == nullptr) {
        return 0.0;
    }
    return ComputeLinearSpeed(query->Motion1) + ComputeLinearSpeed(query->Motion2);
}

NTSTATUS RunBinarySearchCcd(
    const FCL_CONTINUOUS_COLLISION_QUERY* query,
    double tolerance,
    ULONG iterations,
    PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept {
    FCL_TRANSFORM transform1 = {};
    FCL_TRANSFORM transform2 = {};
    BOOLEAN intersecting = FALSE;
    FCL_CONTACT_INFO contact = {};

    double lower = 0.0;
    double upper = 1.0;

    for (ULONG iteration = 0; iteration < iterations; ++iteration) {
        const double mid = (lower + upper) * 0.5;

        NTSTATUS status = FclInterpMotionEvaluate(&query->Motion1, mid, &transform1);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        status = FclInterpMotionEvaluate(&query->Motion2, mid, &transform2);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        status = FclCollisionDetect(
            query->Object1,
            &transform1,
            query->Object2,
            &transform2,
            &intersecting,
            &contact);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        if (intersecting) {
            upper = mid;
        } else {
            lower = mid;
        }

        if ((upper - lower) <= tolerance) {
            break;
        }
    }

    result->Intersecting = intersecting;
    result->TimeOfImpact = intersecting ? upper : 1.0;
    if (intersecting) {
        result->Contact = contact;
    } else {
        RtlZeroMemory(&result->Contact, sizeof(result->Contact));
    }
    return STATUS_SUCCESS;
}

NTSTATUS RunConservativeAdvancement(
    const FCL_CONTINUOUS_COLLISION_QUERY* query,
    double tolerance,
    ULONG iterations,
    double relativeSpeed,
    PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept {
    FCL_TRANSFORM transform1 = {};
    FCL_TRANSFORM transform2 = {};
    BOOLEAN intersecting = FALSE;
    FCL_CONTACT_INFO contact = {};

    double time = 0.0;
    const double safeSpeed = (relativeSpeed > kMinRelativeSpeed) ? relativeSpeed : kMinRelativeSpeed;

    for (ULONG iteration = 0; iteration < iterations && time <= 1.0; ++iteration) {
        NTSTATUS status = FclInterpMotionEvaluate(&query->Motion1, time, &transform1);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        status = FclInterpMotionEvaluate(&query->Motion2, time, &transform2);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        status = FclCollisionDetect(
            query->Object1,
            &transform1,
            query->Object2,
            &transform2,
            &intersecting,
            &contact);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        if (intersecting) {
            result->Intersecting = TRUE;
            result->TimeOfImpact = Clamp01(time);
            result->Contact = contact;
            return STATUS_SUCCESS;
        }

        FCL_DISTANCE_RESULT distanceResult = {};
        status = FclDistanceCompute(
            query->Object1,
            &transform1,
            query->Object2,
            &transform2,
            &distanceResult);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        if (distanceResult.Distance <= tolerance) {
            break;
        }

        double step = distanceResult.Distance / safeSpeed;
        if (step < tolerance) {
            step = tolerance;
        }
        time += step;
        if (time >= 1.0) {
            time = 1.0;
            break;
        }
    }

    result->Intersecting = FALSE;
    result->TimeOfImpact = Clamp01(time);
    RtlZeroMemory(&result->Contact, sizeof(result->Contact));
    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclInterpMotionInitialize(
    _In_ const FCL_INTERP_MOTION_DESC* desc,
    _Out_ PFCL_INTERP_MOTION motion) noexcept {
    if (desc == nullptr || motion == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    motion->Start = desc->Start;
    motion->End = desc->End;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclInterpMotionEvaluate(
    _In_ const FCL_INTERP_MOTION* motion,
    _In_ double t,
    _Out_ PFCL_TRANSFORM transform) noexcept {
    if (motion == nullptr || transform == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    const double clamped = Clamp01(t);
    const FCL_VECTOR3 translation = fclmusa::geom::LerpVector(
        motion->Start.Translation,
        motion->End.Translation,
        clamped);

    const fclmusa::geom::FCL_QUATERNION startQuat =
        fclmusa::geom::QuaternionFromMatrix(motion->Start.Rotation);
    const fclmusa::geom::FCL_QUATERNION endQuat =
        fclmusa::geom::QuaternionFromMatrix(motion->End.Rotation);
    const fclmusa::geom::FCL_QUATERNION interpolated =
        fclmusa::geom::QuaternionSlerp(startQuat, endQuat, clamped);

    transform->Rotation = fclmusa::geom::QuaternionToMatrix(interpolated);
    transform->Translation = translation;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclScrewMotionInitialize(
    _In_ const FCL_SCREW_MOTION_DESC* desc,
    _Out_ PFCL_SCREW_MOTION motion) noexcept {
    if (desc == nullptr || motion == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!fclmusa::geom::IsValidTransform(desc->Start) ||
        !fclmusa::geom::IsValidTransform(desc->End)) {
        return STATUS_INVALID_PARAMETER;
    }

    motion->Start = desc->Start;
    motion->Axis = {1.0f, 0.0f, 0.0f};
    motion->AngularVelocity = 0.0f;
    motion->LinearVelocity = 0.0f;
    motion->OrthogonalTranslation = {0.0f, 0.0f, 0.0f};

    const FCL_MATRIX3X3 startInverse = fclmusa::geom::TransposeMatrix(desc->Start.Rotation);
    const FCL_MATRIX3X3 deltaRotation = fclmusa::geom::MultiplyMatrix(desc->End.Rotation, startInverse);

    FCL_VECTOR3 axis = {1.0f, 0.0f, 0.0f};
    float angle = 0.0f;
    if (fclmusa::geom::AxisAngleFromMatrix(deltaRotation, &axis, &angle)) {
        motion->Axis = axis;
        motion->AngularVelocity = angle;
    }

    const FCL_VECTOR3 deltaTranslation = fclmusa::geom::Subtract(
        desc->End.Translation,
        desc->Start.Translation);
    const float parallel = fclmusa::geom::Dot(deltaTranslation, motion->Axis);
    motion->LinearVelocity = parallel;
    motion->OrthogonalTranslation = fclmusa::geom::Subtract(
        deltaTranslation,
        fclmusa::geom::Scale(motion->Axis, parallel));

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclScrewMotionEvaluate(
    _In_ const FCL_SCREW_MOTION* motion,
    _In_ double t,
    _Out_ PFCL_TRANSFORM transform) noexcept {
    if (motion == nullptr || transform == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    const float clamped = static_cast<float>(Clamp01(t));

    const FCL_MATRIX3X3 deltaRotation = fclmusa::geom::RotationMatrixFromAxisAngle(
        motion->Axis,
        motion->AngularVelocity * clamped);
    transform->Rotation = fclmusa::geom::MultiplyMatrix(deltaRotation, motion->Start.Rotation);

    FCL_VECTOR3 translation = motion->Start.Translation;
    translation = fclmusa::geom::Add(translation, fclmusa::geom::Scale(motion->OrthogonalTranslation, clamped));
    translation = fclmusa::geom::Add(translation, fclmusa::geom::Scale(motion->Axis, motion->LinearVelocity * clamped));
    transform->Translation = translation;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclContinuousCollision(
    _In_ const FCL_CONTINUOUS_COLLISION_QUERY* query,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept {
    if (query == nullptr || result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    const double tolerance = (query->Tolerance > 0.0) ? query->Tolerance : kDefaultTolerance;
    const ULONG iterations = (query->MaxIterations > 0) ? query->MaxIterations : kDefaultIterations;

    const double relativeSpeed = EstimateRelativeSpeed(query);
    if (relativeSpeed <= kMinRelativeSpeed) {
        return RunBinarySearchCcd(query, tolerance, iterations, result);
    }
    return RunConservativeAdvancement(query, tolerance, iterations, relativeSpeed, result);
}
