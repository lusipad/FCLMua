#include <cmath>

#include "fclmusa/collision.h"
#include "fclmusa/distance.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/ioctl.h"
#include "fclmusa/logging.h"
#include "fclmusa/platform.h"

using fclmusa::geom::IdentityTransform;

namespace {

constexpr float kTolerance = 1e-4f;

struct GeometryHandle {
    FCL_GEOMETRY_HANDLE handle = {};

    ~GeometryHandle() {
        Release();
    }

    void Release() noexcept {
        if (FclIsGeometryHandleValid(handle)) {
            FclDestroyGeometry(handle);
            handle.Value = 0;
        }
    }
};

NTSTATUS CreateSphere(float radius, GeometryHandle& out) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = radius;
    out.Release();
    return FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &out.handle);
}

bool RunInvalidDescriptorTest() noexcept {
    FCL_GEOMETRY_HANDLE unused = {};
    const NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, nullptr, &unused);
    return status == STATUS_INVALID_PARAMETER;
}

bool VerifyCollisionResult(const GeometryHandle& a,
    const GeometryHandle& b,
    float separation,
    BOOLEAN expectedCollision,
    float expectedPenetration) noexcept {
    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation.X = separation;

    BOOLEAN isColliding = FALSE;
    FCL_CONTACT_INFO contact = {};
    const NTSTATUS status = FclCollisionDetect(
        a.handle,
        &transformA,
        b.handle,
        &transformB,
        &isColliding,
        &contact);
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("FclCollisionDetect failed: 0x%X", status);
        return false;
    }

    if (isColliding != expectedCollision) {
        FCL_LOG_ERROR(
            "Collision mismatch for separation %.3f (got %d expected %d)",
            separation,
            isColliding,
            expectedCollision);
        return false;
    }

    if (expectedCollision) {
        if (std::fabs(contact.PenetrationDepth - expectedPenetration) > kTolerance) {
            FCL_LOG_ERROR(
                "Penetration mismatch %.6f vs %.6f",
                contact.PenetrationDepth,
                expectedPenetration);
            return false;
        }
    } else if (std::fabs(contact.PenetrationDepth) > kTolerance) {
        FCL_LOG_ERROR("Expected zero penetration but got %.6f", contact.PenetrationDepth);
        return false;
    }

    return true;
}

bool RunSphereCollisionSuite() noexcept {
    GeometryHandle sphereA;
    GeometryHandle sphereB;
    if (!NT_SUCCESS(CreateSphere(1.0f, sphereA))) {
        return false;
    }
    if (!NT_SUCCESS(CreateSphere(1.5f, sphereB))) {
        return false;
    }

    const float sumRadius = 2.5f;
    if (!VerifyCollisionResult(sphereA, sphereB, sumRadius + 1.0f, FALSE, 0.0f)) {
        return false;
    }
    if (!VerifyCollisionResult(sphereA, sphereB, sumRadius, TRUE, 0.0f)) {
        return false;
    }
    if (!VerifyCollisionResult(sphereA, sphereB, sumRadius - 0.75f, TRUE, 0.75f)) {
        return false;
    }
    return true;
}

bool RunDistanceSuite() noexcept {
    GeometryHandle sphereA;
    GeometryHandle sphereB;
    if (!NT_SUCCESS(CreateSphere(1.0f, sphereA))) {
        return false;
    }
    if (!NT_SUCCESS(CreateSphere(1.0f, sphereB))) {
        return false;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation.X = 4.0f;

    FCL_DISTANCE_RESULT result = {};
    const NTSTATUS status = FclDistanceCompute(
        sphereA.handle,
        &transformA,
        sphereB.handle,
        &transformB,
        &result);
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("FclDistanceCompute failed: 0x%X", status);
        return false;
    }

    const float expectedDistance = 2.0f;
    if (std::fabs(result.Distance - expectedDistance) > kTolerance) {
        FCL_LOG_ERROR("Distance mismatch %.6f vs %.6f", result.Distance, expectedDistance);
        return false;
    }

    const float observedSpan = result.ClosestPoint2.X - result.ClosestPoint1.X;
    if (std::fabs(observedSpan - expectedDistance) > kTolerance) {
        FCL_LOG_ERROR("Closest-point span mismatch %.6f vs %.6f", observedSpan, expectedDistance);
        return false;
    }

    return true;
}

}  // namespace

int main() {
    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("FclGeometrySubsystemInitialize failed: 0x%X", status);
        return 1;
    }

    struct SubsystemGuard {
        ~SubsystemGuard() {
            FclGeometrySubsystemShutdown();
        }
    } guard;

    if (!RunInvalidDescriptorTest()) {
        return 10;
    }
    if (!RunSphereCollisionSuite()) {
        return 11;
    }
    if (!RunDistanceSuite()) {
        return 12;
    }

    return 0;
}
