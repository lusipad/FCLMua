#include <ntddk.h>
#include <wdm.h>

#include <memory>

#include "fclmusa/upstream/upstream_bridge.h"
#include "fclmusa/upstream/geometry_bridge.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/logging.h"
#include "fclmusa/test/assertions.h"
#include "fclmusa/upstream/upstream_tests.h"

namespace {

using fclmusa::geom::IdentityTransform;

FCL_SPHERE_GEOMETRY_DESC MakeSphereDesc(float radius) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = radius;
    return desc;
}

FCL_GEOMETRY_SNAPSHOT MakeSphereSnapshot(float radius) noexcept {
    FCL_GEOMETRY_SNAPSHOT snapshot = {};
    snapshot.Type = FCL_GEOMETRY_SPHERE;
    snapshot.Data.Sphere = MakeSphereDesc(radius);
    return snapshot;
}

NTSTATUS RunCombineTransformTests() noexcept {
    FCL_TRANSFORM parent = IdentityTransform();
    parent.Translation = {1.0f, 2.0f, 3.0f};

    FCL_TRANSFORM child = IdentityTransform();
    child.Translation = {4.0f, 5.0f, 6.0f};

    FCL_TRANSFORM combined = fclmusa::upstream::CombineTransforms(parent, child);
    FCL_TEST_EXPECT_TRUE(combined.Translation.X == 5.0f, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(combined.Translation.Y == 7.0f, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(combined.Translation.Z == 9.0f, STATUS_DATA_ERROR);
    return STATUS_SUCCESS;
}

NTSTATUS RunGeometryBindingTests() noexcept {
    FCL_GEOMETRY_SNAPSHOT snapshot = MakeSphereSnapshot(1.0f);
    fclmusa::upstream::GeometryBinding binding = {};
    FCL_TEST_EXPECT_STATUS(
        fclmusa::upstream::BuildGeometryBinding(snapshot, nullptr),
        STATUS_INVALID_PARAMETER);
    FCL_TEST_EXPECT_NT_SUCCESS(fclmusa::upstream::BuildGeometryBinding(snapshot, &binding));
    FCL_TEST_EXPECT_TRUE(binding.Geometry != nullptr, STATUS_DATA_ERROR);

    snapshot.Data.Sphere.Radius = -1.0f;
    FCL_TEST_EXPECT_STATUS(
        fclmusa::upstream::BuildGeometryBinding(snapshot, &binding),
        STATUS_INVALID_PARAMETER);
    return STATUS_SUCCESS;
}

NTSTATUS RunCollisionObjectBuildingTest() noexcept {
    FCL_GEOMETRY_SNAPSHOT sphereA = MakeSphereSnapshot(1.0f);
    FCL_GEOMETRY_SNAPSHOT sphereB = MakeSphereSnapshot(1.0f);
    FCL_TRANSFORM tfA = IdentityTransform();
    FCL_TRANSFORM tfB = IdentityTransform();
    tfB.Translation = {5.0f, 0.0f, 0.0f};

    fclmusa::upstream::CollisionObjects objects = {};
    fcl::Transform3d eigenA = fcl::Transform3d::Identity();
    fcl::Transform3d eigenB = fcl::Transform3d::Identity();

    FCL_TEST_EXPECT_NT_SUCCESS(
        fclmusa::upstream::BuildCollisionObjects(
            sphereA,
            tfA,
            sphereB,
            tfB,
            &objects,
            eigenA,
            eigenB));
    FCL_TEST_EXPECT_TRUE(objects.Object1.Geometry != nullptr, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(objects.Object2.Geometry != nullptr, STATUS_DATA_ERROR);
    return STATUS_SUCCESS;
}

NTSTATUS RunUpstreamBridgeSmokeTest() noexcept {
    FCL_GEOMETRY_SNAPSHOT sphereA = MakeSphereSnapshot(1.0f);
    FCL_GEOMETRY_SNAPSHOT sphereB = MakeSphereSnapshot(1.5f);
    FCL_TRANSFORM tfA = IdentityTransform();
    FCL_TRANSFORM tfB = IdentityTransform();
    tfB.Translation = {0.0f, 0.0f, 3.0f};

    BOOLEAN isColliding = TRUE;
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclUpstreamCollide(
            sphereA,
            tfA,
            sphereB,
            tfB,
            &isColliding,
            nullptr));
    FCL_TEST_EXPECT_FALSE(isColliding, STATUS_DATA_ERROR);

    FCL_DISTANCE_RESULT distanceResult = {};
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclUpstreamDistance(
            sphereA,
            tfA,
            sphereB,
            tfB,
            &distanceResult));
    FCL_TEST_EXPECT_TRUE(distanceResult.Distance > 0.0f, STATUS_DATA_ERROR);

    FCL_INTERP_MOTION motionA = {};
    motionA.Start = IdentityTransform();
    motionA.End = IdentityTransform();
    motionA.End.Translation.X = 4.0f;

    FCL_INTERP_MOTION motionB = {};
    motionB.Start = IdentityTransform();
    motionB.Start.Translation.X = 6.0f;
    motionB.End = motionB.Start;

    FCL_CONTINUOUS_COLLISION_RESULT ccdResult = {};
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclUpstreamContinuousCollision(
            sphereA,
            motionA,
            sphereB,
            motionB,
            1e-4,
            32,
            &ccdResult));
    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunUpstreamBridgeTests() noexcept {
    FCL_TEST_EXPECT_NT_SUCCESS(RunCombineTransformTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunGeometryBindingTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunCollisionObjectBuildingTest());
    FCL_TEST_EXPECT_NT_SUCCESS(RunUpstreamBridgeSmokeTest());
    return STATUS_SUCCESS;
}

