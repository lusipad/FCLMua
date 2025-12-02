#include <ntddk.h>
#include <wdm.h>

#include <cstdint>
#include <cstring>

#include "fclmusa/logging.h"
#include "fclmusa/memory/pool_allocator.h"
#include "fclmusa/memory/pool_test.h"
#include "fclmusa/test/assertions.h"

namespace {

constexpr ULONG kTestPoolTag = 'tseT';

struct ScopedAllocation {
    void* pointer = nullptr;

    ~ScopedAllocation() {
        Reset();
    }

    void Reset(void* newPointer = nullptr) noexcept {
        if (pointer != nullptr) {
            fclmusa::memory::Free(pointer, kTestPoolTag);
        }
        pointer = newPointer;
    }

    void* Release() noexcept {
        void* current = pointer;
        pointer = nullptr;
        return current;
    }
};

ULONGLONG AbsoluteDifference(ULONGLONG a, ULONGLONG b) noexcept {
    return (a >= b) ? (a - b) : (b - a);
}

NTSTATUS RunAllocationAccountingTest() noexcept {
    const auto before = fclmusa::memory::QueryStats();

    ScopedAllocation first;
    ScopedAllocation second;

    first.Reset(fclmusa::memory::Allocate(64, kTestPoolTag));
    FCL_TEST_EXPECT_NOT_NULL(first.pointer, STATUS_INSUFFICIENT_RESOURCES);
    FCL_TEST_EXPECT_TRUE(fclmusa::memory::QueryAllocationSize(first.pointer) == 64, STATUS_DATA_ERROR);

    second.Reset(fclmusa::memory::Allocate(128, kTestPoolTag));
    FCL_TEST_EXPECT_NOT_NULL(second.pointer, STATUS_INSUFFICIENT_RESOURCES);
    FCL_TEST_EXPECT_TRUE(fclmusa::memory::QueryAllocationSize(second.pointer) == 128, STATUS_DATA_ERROR);

    const auto mid = fclmusa::memory::QueryStats();
    FCL_TEST_EXPECT_TRUE(mid.BytesInUse >= before.BytesInUse, STATUS_DATA_ERROR);
    const ULONGLONG midDelta = mid.BytesInUse - before.BytesInUse;
    FCL_TEST_EXPECT_TRUE(midDelta == 64 + 128, STATUS_DATA_ERROR);

    first.Reset();
    second.Reset();

    const auto after = fclmusa::memory::QueryStats();
    FCL_TEST_EXPECT_TRUE(after.BytesInUse == before.BytesInUse, STATUS_DATA_ERROR);

    const ULONGLONG allocationDelta = after.AllocationCount - before.AllocationCount;
    FCL_TEST_EXPECT_TRUE(allocationDelta >= 2, STATUS_DATA_ERROR);

    const ULONGLONG bytesAllocatedDelta = after.BytesAllocated - before.BytesAllocated;
    FCL_TEST_EXPECT_TRUE(bytesAllocatedDelta == 64 + 128, STATUS_DATA_ERROR);

    const ULONGLONG freeCountDelta = after.FreeCount - before.FreeCount;
    FCL_TEST_EXPECT_TRUE(freeCountDelta >= 2, STATUS_DATA_ERROR);

    const ULONGLONG bytesFreedDelta = after.BytesFreed - before.BytesFreed;
    FCL_TEST_EXPECT_TRUE(bytesFreedDelta == 64 + 128, STATUS_DATA_ERROR);

    return STATUS_SUCCESS;
}

NTSTATUS RunReallocatePreservesContentTest() noexcept {
    const auto before = fclmusa::memory::QueryStats();

    ScopedAllocation allocation;
    allocation.Reset(fclmusa::memory::Allocate(32, kTestPoolTag));
    FCL_TEST_EXPECT_NOT_NULL(allocation.pointer, STATUS_INSUFFICIENT_RESOURCES);

    auto* bytes = static_cast<std::uint8_t*>(allocation.pointer);
    for (std::uint8_t i = 0; i < 32; ++i) {
        bytes[i] = i;
    }

    void* grownBuffer = fclmusa::memory::Reallocate(allocation.Release(), 64, kTestPoolTag);
    FCL_TEST_EXPECT_NOT_NULL(grownBuffer, STATUS_INSUFFICIENT_RESOURCES);
    allocation.Reset(grownBuffer);

    auto* grownBytes = static_cast<std::uint8_t*>(allocation.pointer);
    for (std::uint8_t i = 0; i < 32; ++i) {
        FCL_TEST_EXPECT_TRUE(grownBytes[i] == i, STATUS_DATA_ERROR);
    }
    FCL_TEST_EXPECT_TRUE(fclmusa::memory::QueryAllocationSize(allocation.pointer) == 64, STATUS_DATA_ERROR);

    allocation.Reset();

    const auto after = fclmusa::memory::QueryStats();
    FCL_TEST_EXPECT_TRUE(after.BytesInUse == before.BytesInUse, STATUS_DATA_ERROR);

    const ULONGLONG allocationDelta = after.BytesAllocated - before.BytesAllocated;
    const ULONGLONG freedDelta = after.BytesFreed - before.BytesFreed;
    FCL_TEST_EXPECT_TRUE(allocationDelta == freedDelta, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(allocationDelta == 32 + 64, STATUS_DATA_ERROR);

    return STATUS_SUCCESS;
}

NTSTATUS RunZeroSizeReallocateTest() noexcept {
    const auto before = fclmusa::memory::QueryStats();

    ScopedAllocation allocation;
    allocation.Reset(fclmusa::memory::Allocate(48, kTestPoolTag));
    FCL_TEST_EXPECT_NOT_NULL(allocation.pointer, STATUS_INSUFFICIENT_RESOURCES);

    void* released = allocation.Release();
    void* shrunk = fclmusa::memory::Reallocate(released, 0, kTestPoolTag);
    FCL_TEST_EXPECT_NULL(shrunk, STATUS_DATA_ERROR);

    const auto after = fclmusa::memory::QueryStats();
    FCL_TEST_EXPECT_TRUE(after.BytesInUse == before.BytesInUse, STATUS_DATA_ERROR);

    const ULONGLONG freedDelta = after.BytesFreed - before.BytesFreed;
    FCL_TEST_EXPECT_TRUE(freedDelta >= 48, STATUS_DATA_ERROR);

    return STATUS_SUCCESS;
}

NTSTATUS RunHighIrqlGuardTest() noexcept {
    const auto before = fclmusa::memory::QueryStats();

    KIRQL oldIrql = PASSIVE_LEVEL;
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    void* buffer = fclmusa::memory::Allocate(16, kTestPoolTag);
    KeLowerIrql(oldIrql);

    FCL_TEST_EXPECT_NULL(buffer, STATUS_DATA_ERROR);

    const auto after = fclmusa::memory::QueryStats();
    FCL_TEST_EXPECT_TRUE(after.AllocationCount == before.AllocationCount, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(after.BytesAllocated == before.BytesAllocated, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(after.BytesInUse == before.BytesInUse, STATUS_DATA_ERROR);

    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclRunMemoryPoolTests() noexcept {
    fclmusa::memory::EnablePoolTracking(TRUE);

    const auto baseline = fclmusa::memory::QueryStats();

    FCL_TEST_EXPECT_NT_SUCCESS(RunAllocationAccountingTest());
    FCL_TEST_EXPECT_NT_SUCCESS(RunReallocatePreservesContentTest());
    FCL_TEST_EXPECT_NT_SUCCESS(RunZeroSizeReallocateTest());
    FCL_TEST_EXPECT_NT_SUCCESS(RunHighIrqlGuardTest());

    const auto finalStats = fclmusa::memory::QueryStats();
    FCL_TEST_EXPECT_TRUE(finalStats.BytesInUse == baseline.BytesInUse, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(finalStats.BytesAllocated >= baseline.BytesAllocated, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(finalStats.BytesFreed >= baseline.BytesFreed, STATUS_DATA_ERROR);
    FCL_TEST_EXPECT_TRUE(AbsoluteDifference(finalStats.BytesAllocated, finalStats.BytesFreed) ==
                             AbsoluteDifference(baseline.BytesAllocated, baseline.BytesFreed),
        STATUS_DATA_ERROR);

    return STATUS_SUCCESS;
}

