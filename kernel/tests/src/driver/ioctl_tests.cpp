#include <ntddk.h>
#include <wdm.h>

#include "fclmusa/driver/ioctl_tests.h"
#include "fclmusa/ioctl.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/collision.h"
#include "fclmusa/distance.h"
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

NTSTATUS RunCollisionQueryValidation() noexcept {
    FCL_COLLISION_IO_BUFFER buffer = {};
    NTSTATUS status = FclCollisionDetect({0}, nullptr, {0}, nullptr, nullptr, nullptr);
    UNREFERENCED_PARAMETER(status);

    buffer.Query.Object1.Value = 0;
    buffer.Query.Object2.Value = 0;
    buffer.Query.Transform1 = IdentityTransform();
    buffer.Query.Transform2 = IdentityTransform();

    NTSTATUS collisionStatus = FclCollisionDetect(
        buffer.Query.Object1,
        &buffer.Query.Transform1,
        buffer.Query.Object2,
        &buffer.Query.Transform2,
        nullptr,
        nullptr);
    UNREFERENCED_PARAMETER(collisionStatus);

    // Validate driver handler behavior
    if (buffer.Query.Object1.Value == 0 || buffer.Query.Object2.Value == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS RunDistanceQueryValidation() noexcept {
    FCL_DISTANCE_IO_BUFFER buffer = {};
    buffer.Query.Object1.Value = 0;
    buffer.Query.Object2.Value = 0;
    buffer.Query.Transform1 = IdentityTransform();
    buffer.Query.Transform2 = IdentityTransform();

    if (buffer.Query.Object1.Value == 0 || buffer.Query.Object2.Value == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS RunCollisionScenario() noexcept {
    GeometryHandleGuard sphereA;
    GeometryHandleGuard sphereB;
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, sphereA));
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, sphereB));

    FCL_COLLISION_IO_BUFFER buffer = {};
    buffer.Query.Object1 = sphereA.handle;
    buffer.Query.Transform1 = IdentityTransform();
    buffer.Query.Object2 = sphereB.handle;
    buffer.Query.Transform2 = IdentityTransform();
    buffer.Query.Transform2.Translation.X = 5.0f;

    BOOLEAN isColliding = FALSE;
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclCollisionDetect(
            buffer.Query.Object1,
            &buffer.Query.Transform1,
            buffer.Query.Object2,
            &buffer.Query.Transform2,
            &isColliding,
            &buffer.Result.Contact));
    FCL_TEST_EXPECT_FALSE(isColliding, STATUS_DATA_ERROR);
    buffer.Result.IsColliding = isColliding ? 1 : 0;
    return STATUS_SUCCESS;
}

NTSTATUS RunDistanceScenario() noexcept {
    GeometryHandleGuard sphereA;
    GeometryHandleGuard sphereB;
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, sphereA));
    FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, sphereB));

    FCL_DISTANCE_IO_BUFFER buffer = {};
    buffer.Query.Object1 = sphereA.handle;
    buffer.Query.Transform1 = IdentityTransform();
    buffer.Query.Object2 = sphereB.handle;
    buffer.Query.Transform2 = IdentityTransform();
    buffer.Query.Transform2.Translation.X = 4.0f;

    FCL_DISTANCE_RESULT distance = {};
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclDistanceCompute(
            buffer.Query.Object1,
            &buffer.Query.Transform1,
            buffer.Query.Object2,
            &buffer.Query.Transform2,
            &distance));
    buffer.Result.Distance = distance.Distance;
    buffer.Result.ClosestPoint1 = distance.ClosestPoint1;
    buffer.Result.ClosestPoint2 = distance.ClosestPoint2;
    return STATUS_SUCCESS;
}

NTSTATUS RunCreateDestroyTests() noexcept {
    FCL_CREATE_SPHERE_INPUT input = {};
    input.Desc = MakeSphereDesc(1.0f);
    FCL_CREATE_SPHERE_OUTPUT output = {};

    FCL_TEST_EXPECT_NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_SPHERE, &input.Desc, &output.Handle));
    FCL_TEST_EXPECT_TRUE(FclIsGeometryHandleValid(output.Handle), STATUS_INTERNAL_ERROR);

    FCL_DESTROY_INPUT destroyInput = {};
    destroyInput.Handle = output.Handle;
    FCL_TEST_EXPECT_NT_SUCCESS(FclDestroyGeometry(destroyInput.Handle));
    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunDriverIoctlTests() noexcept {
    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    FCL_TEST_EXPECT_NT_SUCCESS(RunCollisionQueryValidation());
    FCL_TEST_EXPECT_NT_SUCCESS(RunDistanceQueryValidation());
    FCL_TEST_EXPECT_NT_SUCCESS(RunCollisionScenario());
    FCL_TEST_EXPECT_NT_SUCCESS(RunDistanceScenario());
    FCL_TEST_EXPECT_NT_SUCCESS(RunCreateDestroyTests());

    return STATUS_SUCCESS;
}

