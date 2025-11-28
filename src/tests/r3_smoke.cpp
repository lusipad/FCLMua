#include "fclmusa/geometry.h"
#include "fclmusa/platform.h"

int main() {
    // Initialize subsystem (no-op reentry on success).
    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        return 1;
    }

    FCL_SPHERE_GEOMETRY_DESC sphere = {};
    sphere.Center = {0.0f, 0.0f, 0.0f};
    sphere.Radius = 1.0f;

    FCL_GEOMETRY_HANDLE handle = {};
    status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphere, &handle);
    if (!NT_SUCCESS(status) || !FclIsGeometryHandleValid(handle)) {
        FclGeometrySubsystemShutdown();
        return 2;
    }

    NTSTATUS destroyStatus = FclDestroyGeometry(handle);
    FclGeometrySubsystemShutdown();
    return NT_SUCCESS(destroyStatus) ? 0 : 3;
}
