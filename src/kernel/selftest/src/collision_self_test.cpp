#include <ntddk.h>
#include <wdm.h>

#include <cmath>

#include "fclmusa/collision.h"
#include "fclmusa/collision/self_test.h"
#include "fclmusa/geometry.h"
#include "fclmusa/logging.h"

namespace {

constexpr float kTolerance = 1e-4f;

FCL_TRANSFORM IdentityTransform() noexcept {
    FCL_TRANSFORM transform = {};
    transform.Rotation.M[0][0] = 1.0f;
    transform.Rotation.M[1][1] = 1.0f;
    transform.Rotation.M[2][2] = 1.0f;
    transform.Translation = {0.0f, 0.0f, 0.0f};
    return transform;
}

NTSTATUS CreateSphere(float radius, _Out_ PFCL_GEOMETRY_HANDLE handle) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = radius;
    return FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, handle);
}

NTSTATUS RunSphereSphereCase(
    _In_z_ const char* label,
    FCL_GEOMETRY_HANDLE handleA,
    FCL_GEOMETRY_HANDLE handleB,
    float separationX,
    BOOLEAN expectedCollision,
    float expectedPenetration) noexcept {
    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation.X = separationX;

    BOOLEAN isColliding = FALSE;
    FCL_CONTACT_INFO contact = {};

    NTSTATUS status = FclCollisionDetect(
        handleA,
        &transformA,
        handleB,
        &transformB,
        &isColliding,
        &contact);

    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Collision smoke test %s failed: 0x%X", label, status);
        return status;
    }

    if (isColliding != expectedCollision) {
        FCL_LOG_ERROR(
            "Collision smoke test %s mismatch colliding=%d expected=%d",
            label,
            isColliding,
            expectedCollision);
        return STATUS_DATA_ERROR;
    }

    if (expectedCollision) {
        if (std::fabs(contact.PenetrationDepth - expectedPenetration) > kTolerance) {
            FCL_LOG_ERROR(
                "Collision smoke test %s penetration mismatch %.6f vs %.6f",
                label,
                contact.PenetrationDepth,
                expectedPenetration);
            return STATUS_DATA_ERROR;
        }
    } else {
        if (std::fabs(contact.PenetrationDepth) > kTolerance) {
            FCL_LOG_ERROR(
                "Collision smoke test %s expected zero penetration, got %.6f",
                label,
                contact.PenetrationDepth);
            return STATUS_DATA_ERROR;
        }
    }

    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunCollisionSmokeTests() noexcept {
    const float radiusA = 1.0f;
    const float radiusB = 1.5f;
    const float sumRadius = radiusA + radiusB;

    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};

    NTSTATUS status = CreateSphere(radiusA, &sphereA);
    if (!NT_SUCCESS(status)) {
        FCL_LOG_ERROR("Collision smoke test: failed to create sphere A: 0x%X", status);
        return status;
    }

    status = CreateSphere(radiusB, &sphereB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphereA);
        FCL_LOG_ERROR("Collision smoke test: failed to create sphere B: 0x%X", status);
        return status;
    }

    status = RunSphereSphereCase("separating", sphereA, sphereB, sumRadius + 1.0f, FALSE, 0.0f);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = RunSphereSphereCase("penetrating", sphereA, sphereB, sumRadius - 1.0f, TRUE, 1.0f);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

    status = RunSphereSphereCase("touching", sphereA, sphereB, sumRadius, TRUE, 0.0f);
    if (!NT_SUCCESS(status)) {
        goto Cleanup;
    }

Cleanup:
    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
    return status;
}

