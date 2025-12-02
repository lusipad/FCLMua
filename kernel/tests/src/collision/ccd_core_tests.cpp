#include <ntddk.h>
#include <wdm.h>

#include "fclmusa/collision.h"
#include "fclmusa/collision/ccd_core_tests.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/test/assertions.h"

namespace {

using fclmusa::geom::IdentityTransform;

FCL_SPHERE_GEOMETRY_DESC MakeSphereDesc(float radius) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = radius;
    return desc;
}

struct GeometryHandleGuard {
    FCL_GEOMETRY_HANDLE handle = {};
    ~GeometryHandleGuard() {
        if (FclIsGeometryHandleValid(handle)) {
            FclDestroyGeometry(handle);
        }
    }
};

NTSTATUS CreateSphere(float radius, GeometryHandleGuard& guard) noexcept {
    auto desc = MakeSphereDesc(radius);
    return FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &guard.handle);
}

NTSTATUS RunInterpMotionTests() noexcept {
    FCL_INTERP_MOTION_DESC desc = {};
    desc.Start = IdentityTransform();
    desc.End = IdentityTransform();

    FCL_INTERP_MOTION motion = {};
    FCL_TEST_EXPECT_STATUS(FclInterpMotionInitialize(nullptr, &motion), STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_STATUS(FclInterpMotionInitialize(&desc, nullptr), STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_NT_SUCCESS(FclInterpMotionInitialize(&desc, &motion));

    FCL_TRANSFORM evaluated = {};
    FCL_TEST_EXPECT_STATUS(FclInterpMotionEvaluate(nullptr, 0.5, &evaluated), STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_STATUS(FclInterpMotionEvaluate(&motion, 0.5, nullptr), STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_NT_SUCCESS(FclInterpMotionEvaluate(&motion, -1.0, &evaluated));
    FCL_TEST_EXPECT_TRUE(evaluated.Translation.X == motion.Start.Translation.X, STATUS_DATA_ERROR);

    FCL_TEST_EXPECT_NT_SUCCESS(FclInterpMotionEvaluate(&motion, 2.0, &evaluated));
    FCL_TEST_EXPECT_TRUE(evaluated.Translation.X == motion.End.Translation.X, STATUS_DATA_ERROR);
    return STATUS_SUCCESS;
}

NTSTATUS RunCoreNullParameterTests() noexcept {
    FCL_CONTINUOUS_COLLISION_RESULT result = {};
    FCL_TEST_EXPECT_STATUS(
        FclContinuousCollisionCoreFromSnapshots(nullptr, nullptr, nullptr, nullptr, 0.0, 0, &result),
        STATUS_INVALID_PARAMETER);
    return STATUS_SUCCESS;
}

NTSTATUS RunQueryValidationTests() noexcept {
    FCL_CONTINUOUS_COLLISION_QUERY query = {};
    FCL_CONTINUOUS_COLLISION_RESULT result = {};
    FCL_TEST_EXPECT_STATUS(FclContinuousCollision(nullptr, &result), STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_STATUS(FclContinuousCollision(&query, nullptr), STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_STATUS(FclContinuousCollision(&query, &result), STATUS_INVALID_HANDLE);

    return STATUS_SUCCESS;
}

NTSTATUS RunIrqlValidationTest() noexcept {
    GeometryHandleGuard sphere;
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, sphere));

    FCL_INTERP_MOTION_DESC motionDesc = {};
    motionDesc.Start = IdentityTransform();
    motionDesc.End = IdentityTransform();
    FCL_INTERP_MOTION motion = {};
    FCL_TEST_EXPECT_NT_SUCCESS(FclInterpMotionInitialize(&motionDesc, &motion));

    FCL_CONTINUOUS_COLLISION_QUERY query = {};
    query.Object1 = sphere.handle;
    query.Object2 = sphere.handle;
    query.Motion1 = motion;
    query.Motion2 = motion;
    query.Tolerance = 1e-4;
    query.MaxIterations = 32;

    FCL_CONTINUOUS_COLLISION_RESULT result = {};
    KIRQL oldIrql = PASSIVE_LEVEL;
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    const NTSTATUS status = FclContinuousCollision(&query, &result);
    KeLowerIrql(oldIrql);
    FCL_TEST_EXPECT_STATUS(status, STATUS_INVALID_DEVICE_STATE);
    return STATUS_SUCCESS;
}

NTSTATUS RunContinuousCollisionSuccessTest() noexcept {
    GeometryHandleGuard moving;
    GeometryHandleGuard stationary;
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, moving));
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, stationary));

    FCL_INTERP_MOTION_DESC moveDesc = {};
    moveDesc.Start = IdentityTransform();
    moveDesc.End = IdentityTransform();
    moveDesc.End.Translation.X = 4.0f;

    FCL_INTERP_MOTION motionMoving = {};
    FCL_INTERP_MOTION motionStationary = {};
    FCL_INTERP_MOTION_DESC stationaryDesc = {};
    stationaryDesc.Start = IdentityTransform();
    stationaryDesc.Start.Translation.X = 6.0f;
    stationaryDesc.End = stationaryDesc.Start;

    FCL_TEST_EXPECT_NT_SUCCESS(FclInterpMotionInitialize(&moveDesc, &motionMoving));
    FCL_TEST_EXPECT_NT_SUCCESS(FclInterpMotionInitialize(&stationaryDesc, &motionStationary));

    FCL_CONTINUOUS_COLLISION_QUERY query = {};
    query.Object1 = moving.handle;
    query.Motion1 = motionMoving;
    query.Object2 = stationary.handle;
    query.Motion2 = motionStationary;
    query.Tolerance = 1e-4;
    query.MaxIterations = 64;

    FCL_CONTINUOUS_COLLISION_RESULT result = {};
    FCL_TEST_EXPECT_NT_SUCCESS(FclContinuousCollision(&query, &result));
    FCL_TEST_EXPECT_TRUE(result.Intersecting != FALSE, STATUS_DATA_ERROR);
    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunCcdCoreTests() noexcept {
    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    FCL_TEST_EXPECT_NT_SUCCESS(RunInterpMotionTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunCoreNullParameterTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunQueryValidationTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunIrqlValidationTest());
    FCL_TEST_EXPECT_NT_SUCCESS(RunContinuousCollisionSuccessTest());

    return STATUS_SUCCESS;
}

