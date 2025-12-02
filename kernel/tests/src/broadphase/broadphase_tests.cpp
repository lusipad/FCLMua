#include <ntddk.h>
#include <wdm.h>

#include <array>
#include <set>

#include "fclmusa/broadphase.h"
#include "fclmusa/geometry.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/broadphase/broadphase_tests.h"
#include "fclmusa/test/assertions.h"

namespace {

using fclmusa::geom::IdentityTransform;

struct GeometryHandleGuard {
    FCL_GEOMETRY_HANDLE handle = {};

    ~GeometryHandleGuard() {
        if (FclIsGeometryHandleValid(handle)) {
            FclDestroyGeometry(handle);
        }
    }
};

FCL_SPHERE_GEOMETRY_DESC MakeSphereDesc(float radius) noexcept {
    FCL_SPHERE_GEOMETRY_DESC desc = {};
    desc.Center = {0.0f, 0.0f, 0.0f};
    desc.Radius = radius;
    return desc;
}

NTSTATUS CreateSphere(float radius, GeometryHandleGuard& guard) noexcept {
    auto desc = MakeSphereDesc(radius);
    return FclCreateGeometry(FCL_GEOMETRY_SPHERE, &desc, &guard.handle);
}

NTSTATUS RunInvalidParameterTests() noexcept {
    ULONG pairCount = 0;

    FCL_TEST_EXPECT_STATUS(
        FclBroadphaseDetect(nullptr, 1, nullptr, 0, &pairCount),
        STATUS_INVALID_PARAMETER);

    FCL_TEST_EXPECT_STATUS(
        FclBroadphaseDetect(nullptr, 0, nullptr, 0, nullptr),
        STATUS_INVALID_PARAMETER);

    KIRQL oldIrql = PASSIVE_LEVEL;
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    const NTSTATUS irqlStatus = FclBroadphaseDetect(nullptr, 0, nullptr, 0, &pairCount);
    KeLowerIrql(oldIrql);
    FCL_TEST_EXPECT_STATUS(irqlStatus, STATUS_INVALID_DEVICE_STATE);

    return STATUS_SUCCESS;
}

NTSTATUS RunInvalidHandleTests() noexcept {
    FCL_BROADPHASE_OBJECT object = {};
    object.Handle.Value = 0;
    FCL_TRANSFORM transform = IdentityTransform();
    object.Transform = &transform;

    ULONG pairCount = 0;
    FCL_TEST_EXPECT_STATUS(
        FclBroadphaseDetect(&object, 1, nullptr, 0, &pairCount),
        STATUS_INVALID_HANDLE);
    return STATUS_SUCCESS;
}

NTSTATUS RunBufferOverflowTests() noexcept {
    std::array<GeometryHandleGuard, 3> guards;
    for (auto& guard : guards) {
        FCL_TEST_EXPECT_NT_SUCCESS(CreateSphere(1.0f, guard));
    }

    std::array<FCL_TRANSFORM, 3> transforms = {
        IdentityTransform(),
        IdentityTransform(),
        IdentityTransform(),
    };
    transforms[1].Translation = {0.1f, 0.0f, 0.0f};
    transforms[2].Translation = {0.2f, 0.0f, 0.0f};

    std::array<FCL_BROADPHASE_OBJECT, 3> objects = {};
    for (size_t i = 0; i < objects.size(); ++i) {
        objects[i].Handle = guards[i].handle;
        objects[i].Transform = &transforms[i];
    }

    FCL_BROADPHASE_PAIR smallBuffer[1] = {};
    ULONG pairCount = 0;
    const NTSTATUS overflowStatus = FclBroadphaseDetect(
        objects.data(),
        static_cast<ULONG>(objects.size()),
        smallBuffer,
        1,
        &pairCount);
    FCL_TEST_EXPECT_STATUS(overflowStatus, STATUS_BUFFER_TOO_SMALL);
    FCL_TEST_EXPECT_TRUE(pairCount == 3, STATUS_DATA_ERROR);

    FCL_BROADPHASE_PAIR fullBuffer[4] = {};
    pairCount = 0;
    FCL_TEST_EXPECT_NT_SUCCESS(
        FclBroadphaseDetect(
            objects.data(),
            static_cast<ULONG>(objects.size()),
            fullBuffer,
            4,
            &pairCount));
    FCL_TEST_EXPECT_TRUE(pairCount == 3, STATUS_DATA_ERROR);

    std::set<ULONGLONG> seen;
    for (ULONG i = 0; i < pairCount; ++i) {
        const auto a = fullBuffer[i].A.Value;
        const auto b = fullBuffer[i].B.Value;
        FCL_TEST_EXPECT_TRUE(a != 0 && b != 0 && a != b, STATUS_DATA_ERROR);
        const ULONGLONG key = (a < b) ? (a << 32) | b : (b << 32) | a;
        seen.insert(key);
    }
    FCL_TEST_EXPECT_TRUE(seen.size() == 3, STATUS_DATA_ERROR);

    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunBroadphaseCoreTests() noexcept {
    NTSTATUS status = FclGeometrySubsystemInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }

    FCL_TEST_EXPECT_NT_SUCCESS(RunInvalidParameterTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunInvalidHandleTests());
    FCL_TEST_EXPECT_NT_SUCCESS(RunBufferOverflowTests());

    return STATUS_SUCCESS;
}

