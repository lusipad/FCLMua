#include <ntddk.h>
#include <wdm.h>

#include <limits>

#include "fclmusa/collision.h"
#include "fclmusa/collision/collision_core_tests.h"
#include "fclmusa/distance.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/logging.h"
#include "fclmusa/test/assertions.h"

namespace {

using fclmusa::geom::IdentityTransform;

FCL_SPHERE_GEOMETRY_DESC MakeSphereDesc(float radius) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = radius;
    return desc;
}

NTSTATUS CreateSphere(float radius, _Out_ PFCL_GEOMETRY_HANDLE handle) noexcept {
    auto desc = MakeSphereDesc(radius);
    return FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, handle);
}

NTSTATUS RunCollisionNullParameterTests() noexcept {
    BOOLEAN isColliding = FALSE;
    FCL_TEST_EXPECT_STATUS(
        FclCollisionDetect({0}, nullptr, {0}, nullptr, nullptr, nullptr),
        STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_STATUS(
        FclCollisionDetect({0}, nullptr, {0}, nullptr, &isColliding, nullptr),
        STATUS_INVALID_HANDLE);
    return STATUS_SUCCESS;
}

NTSTATUS RunDistanceNullParameterTests() noexcept {
    FCL_DISTANCE_RESULT result = {};
    FCL_TEST_EXPECT_STATUS(
        FclDistanceCompute({0}, nullptr, {0}, nullptr, nullptr),
        STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_STATUS(
        FclDistanceCompute({0}, nullptr, {0}, nullptr, &result),
        STATUS_INVALID_HANDLE);
    return STATUS_SUCCESS;
}

NTSTATUS RunCollisionInvalidTransformTests() noexcept {
    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereA));
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereB));

    FCL_TRANSFORM invalidTransform = IdentityTransform();
    invalidTransform.Rotation.M[0][0] = std::numeric_limits<float>::quiet_NaN();

    BOOLEAN isColliding = FALSE;
    FCL_TEST_EXPECT_STATUS(
        FclCollisionDetect(
            sphereA,
            &invalidTransform,
            sphereB,
            nullptr,
            &isColliding,
            nullptr),
        STATUS_INVALID_PARAMETER);

    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return STATUS_SUCCESS;
}

NTSTATUS RunDistanceInvalidTransformTests() noexcept {
    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereA));
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereB));

    FCL_TRANSFORM invalidTransform = IdentityTransform();
    invalidTransform.Rotation.M[2][2] = std::numeric_limits<float>::infinity();

    FCL_DISTANCE_RESULT distanceResult = {};
    FCL_TEST_EXPECT_STATUS(
        FclDistanceCompute(
            sphereA,
            &invalidTransform,
            sphereB,
            nullptr,
            &distanceResult),
        STATUS_INVALID_PARAMETER);

    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return STATUS_SUCCESS;
}

NTSTATUS RunSeparatedCollisionTests() noexcept {
    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereA));
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereB));

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation = {5.0f, 0.0f, 0.0f};

    BOOLEAN isColliding = TRUE;
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclCollisionDetect(
            sphereA,
            &transformA,
            sphereB,
            &transformB,
            &isColliding,
            nullptr));
    FCL_TEST_EXPECT_FALSE(isColliding, STATUS_DATA_ERROR);

    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return STATUS_SUCCESS;
}

NTSTATUS RunDistanceSeparatedTests() noexcept {
    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereA));
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, &sphereB));

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation = {5.0f, 0.0f, 0.0f};

    FCL_DISTANCE_RESULT distanceResult = {};
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclDistanceCompute(
            sphereA,
            &transformA,
            sphereB,
            &transformB,
            &distanceResult));

    const float expectedDistance = 3.0f;
    FCL_TEST_EXPECT_FLOAT_NEAR(distanceResult.Distance, expectedDistance, 1e-4f, STATUS_DATA_ERROR);

    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunCollisionCoreTests() noexcept {
    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    FCL_TEST_EXPECT_NT_SUCCESS(RunCollisionNullParameterTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunCollisionInvalidTransformTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunSeparatedCollisionTests());
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclRunDistanceCoreTests() noexcept {
    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    FCL_TEST_EXPECT_NT_SUCCESS(RunDistanceNullParameterTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunDistanceInvalidTransformTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunDistanceSeparatedTests());
    return STATUS_SUCCESS;
}

