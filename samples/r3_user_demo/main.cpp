#include <cstdio>
#include <cstdlib>

#include "fclmusa/collision.h"
#include "fclmusa/distance.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/platform.h"

namespace {

void PrintVector(const char* label, const FCL_VECTOR3& value) {
    std::printf("%s (%.3f, %.3f, %.3f)\n", label, value.X, value.Y, value.Z);
}

NTSTATUS CreateSphere(float x, float y, float z, float radius, FCL_GEOMETRY_HANDLE* handle) {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {x, y, z};
    desc.Radius = radius;
    return FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, handle);
}

}  // namespace

int main() {
    std::puts("FclMusa R3 demo (no driver required)");

    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        std::fprintf(stderr, "Failed to initialize geometry subsystem (0x%08lX)\n", status);
        return EXIT_FAILURE;
    }

    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    status = CreateSphere(0.0f, 0.0f, 0.0f, 0.5f, &sphereA);
    if (!NT_SUCCESS(status)) {
        std::fprintf(stderr, "Create sphere A failed (0x%08lX)\n", status);
        FclGeometrySubsystemShutdown();
        return EXIT_FAILURE;
    }
    status = CreateSphere(0.0f, 0.0f, 0.0f, 0.5f, &sphereB);
    if (!NT_SUCCESS(status)) {
        std::fprintf(stderr, "Create sphere B failed (0x%08lX)\n", status);
        FclDestroyGeometry(sphereA);
        FclGeometrySubsystemShutdown();
        return EXIT_FAILURE;
    }

    const auto cleanup = [&]() {
        FclDestroyGeometry(sphereB);
        FclDestroyGeometry(sphereA);
        FclGeometrySubsystemShutdown();
    };

    using fclmusa::geom::IdentityTransform;

    FCL_TRANSFORM poseA = IdentityTransform();
    FCL_TRANSFORM poseB = IdentityTransform();
    poseB.Translation = {0.6f, 0.0f, 0.0f};  // Slight overlap -> collision.

    BOOLEAN intersecting = FALSE;
    FCL_CONTACT_INFO contact = {};
    status = FclCollisionDetect(sphereA, &poseA, sphereB, &poseB, &intersecting, &contact);
    if (!NT_SUCCESS(status)) {
        std::fprintf(stderr, "FclCollisionDetect failed (0x%08lX)\n", status);
        cleanup();
        return EXIT_FAILURE;
    }

    if (intersecting) {
        std::puts("[Collide] spheres are intersecting");
        PrintVector("  Contact normal:", contact.Normal);
        PrintVector("  Point A:", contact.PointOnObject1);
        PrintVector("  Point B:", contact.PointOnObject2);
        std::printf("  Penetration: %.4f\n", contact.PenetrationDepth);
    } else {
        std::puts("[Collide] spheres are separated");
    }

    poseB.Translation = {2.0f, 0.0f, 0.0f};
    FCL_DISTANCE_RESULT distance = {};
    status = FclDistanceCompute(sphereA, &poseA, sphereB, &poseB, &distance);
    if (!NT_SUCCESS(status)) {
        std::fprintf(stderr, "FclDistanceCompute failed (0x%08lX)\n", status);
        cleanup();
        return EXIT_FAILURE;
    }

    std::printf("[Distance] separation = %.3f\n", distance.Distance);
    PrintVector("  Closest point A:", distance.ClosestPoint1);
    PrintVector("  Closest point B:", distance.ClosestPoint2);

    cleanup();
    return EXIT_SUCCESS;
}
