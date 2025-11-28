#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fclmusa/platform.h"

#if FCL_MUSA_KERNEL_MODE
    // Kernel-mode dependencies are pulled in through platform.h.
#else
    #include <atomic>
    #include <cstdlib>
    #include <cstring>
    #include <limits>
    #include <mutex>
    #include <new>
#endif
    #include <algorithm>

#include "fclmusa/logging.h"
#include "fclmusa/memory/pool_allocator.h"

namespace {

struct alignas(void*) AllocationHeader {
    size_t Size;
    ULONG Tag;
};

#if FCL_MUSA_KERNEL_MODE
EX_PUSH_LOCK g_PoolLock = 0;  // Will be initialized in InitializePoolTracking
volatile LONG64 g_AllocationCount = 0;
volatile LONG64 g_FreeCount = 0;
volatile LONG64 g_BytesAllocated = 0;
volatile LONG64 g_BytesFreed = 0;
volatile LONG64 g_BytesInUse = 0;
volatile LONG64 g_PeakBytesInUse = 0;
BOOLEAN g_TrackingEnabled = FALSE;
#else
std::mutex g_PoolMutex;
std::atomic<long long> g_AllocationCount{0};
std::atomic<long long> g_FreeCount{0};
std::atomic<long long> g_BytesAllocated{0};
std::atomic<long long> g_BytesFreed{0};
std::atomic<long long> g_BytesInUse{0};
std::atomic<long long> g_PeakBytesInUse{0};
bool g_TrackingEnabled = false;
#endif

void ResetStatsUnsafe() {
    g_AllocationCount = 0;
    g_FreeCount = 0;
    g_BytesAllocated = 0;
    g_BytesFreed = 0;
    g_BytesInUse = 0;
    g_PeakBytesInUse = 0;
}

void UpdatePeak(long long inUse) {
    long long previous = g_PeakBytesInUse;
    while (inUse > previous) {
#if FCL_MUSA_KERNEL_MODE
        const long long observed = InterlockedCompareExchange64(&g_PeakBytesInUse, inUse, previous);
        if (observed == previous) {
            break;
        }
        previous = observed;
#else
        long long expected = previous;
        if (g_PeakBytesInUse.compare_exchange_strong(expected, inUse)) {
            break;
        }
        previous = expected;
#endif
    }
}

size_t RequestedSizeWithHeader(size_t payload) {
#if FCL_MUSA_KERNEL_MODE
    size_t total = 0;
    if (!NT_SUCCESS(RtlSizeTAdd(payload, sizeof(AllocationHeader), &total))) {
        return 0;
    }
    return total;
#else
    if (payload > (std::numeric_limits<size_t>::max)() - sizeof(AllocationHeader)) {
        return 0;
    }
    return payload + sizeof(AllocationHeader);
#endif
}

}  // namespace

namespace fclmusa::memory {

void InitializePoolTracking() {
#if FCL_MUSA_KERNEL_MODE
    ExInitializePushLock(&g_PoolLock);
    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PoolLock);
    ResetStatsUnsafe();
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PoolLock);
    g_TrackingEnabled = TRUE;
#else
    std::lock_guard<std::mutex> guard(g_PoolMutex);
    ResetStatsUnsafe();
    g_TrackingEnabled = true;
#endif
}

void ShutdownPoolTracking() {
#if FCL_MUSA_KERNEL_MODE
    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PoolLock);
    g_TrackingEnabled = FALSE;
    ResetStatsUnsafe();
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PoolLock);
#else
    std::lock_guard<std::mutex> guard(g_PoolMutex);
    g_TrackingEnabled = false;
    ResetStatsUnsafe();
#endif
}

void EnablePoolTracking(BOOLEAN enable) {
    g_TrackingEnabled = enable ? true : false;
}

void* Allocate(size_t size, ULONG poolTag) noexcept {
    NON_PAGED_CODE;

#if FCL_MUSA_KERNEL_MODE
    if (g_TrackingEnabled && KeGetCurrentIrql() != PASSIVE_LEVEL) {
        FCL_LOG_ERROR("Allocate called at IRQL %u with tracking enabled", KeGetCurrentIrql());
        return nullptr;
    }
#endif

    const size_t totalSize = RequestedSizeWithHeader(size);
    if (totalSize == 0) {
        return nullptr;
    }

    void* raw = nullptr;
#if FCL_MUSA_KERNEL_MODE
    #if defined(POOL_FLAG_NON_PAGED)
        raw = ExAllocatePool2(POOL_FLAG_NON_PAGED, totalSize, poolTag);
    #else
        raw = ExAllocatePoolWithTag(NonPagedPoolNx, totalSize, poolTag);
    #endif
#else
    raw = std::malloc(totalSize);
#endif

    if (raw == nullptr) {
        return nullptr;
    }

    auto* header = reinterpret_cast<AllocationHeader*>(raw);
    header->Size = size;
    header->Tag = poolTag;
    void* payload = header + 1;

    if (g_TrackingEnabled) {
#if FCL_MUSA_KERNEL_MODE
        ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PoolLock);
#else
        std::lock_guard<std::mutex> guard(g_PoolMutex);
#endif
        g_AllocationCount++;
        g_BytesAllocated += static_cast<long long>(header->Size);
        g_BytesInUse += static_cast<long long>(header->Size);
        UpdatePeak(g_BytesInUse);
#if FCL_MUSA_KERNEL_MODE
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PoolLock);
#endif
    }

    return payload;
}

size_t QueryAllocationSize(const void* buffer) noexcept {
    if (buffer == nullptr) {
        return 0;
    }
    auto* header = reinterpret_cast<const AllocationHeader*>(buffer) - 1;
    return header->Size;
}

void* Reallocate(void* buffer, size_t size, ULONG poolTag) noexcept {
    if (buffer == nullptr) {
        return Allocate(size, poolTag);
    }

    if (size == 0) {
        Free(buffer, poolTag);
        return nullptr;
    }

    const size_t totalSize = RequestedSizeWithHeader(size);
    if (totalSize == 0) {
        return nullptr;
    }

    auto* header = reinterpret_cast<AllocationHeader*>(buffer) - 1;
    const size_t bytesToCopy = (std::min)(size, header->Size);

    void* raw = nullptr;
#if FCL_MUSA_KERNEL_MODE
    #if defined(POOL_FLAG_NON_PAGED)
        raw = ExAllocatePool2(POOL_FLAG_NON_PAGED, totalSize, poolTag);
    #else
        raw = ExAllocatePoolWithTag(NonPagedPoolNx, totalSize, poolTag);
    #endif
#else
    raw = std::malloc(totalSize);
#endif

    if (raw == nullptr) {
        return nullptr;
    }

    auto* newHeader = reinterpret_cast<AllocationHeader*>(raw);
    newHeader->Size = size;
    newHeader->Tag = poolTag;
    void* newBuffer = newHeader + 1;
    std::memcpy(newBuffer, buffer, bytesToCopy);

    if (g_TrackingEnabled) {
#if FCL_MUSA_KERNEL_MODE
        ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PoolLock);
#else
        std::lock_guard<std::mutex> guard(g_PoolMutex);
#endif
        g_AllocationCount++;
        g_BytesAllocated += static_cast<long long>(newHeader->Size);
        g_BytesInUse += static_cast<long long>(newHeader->Size);
        UpdatePeak(g_BytesInUse);
#if FCL_MUSA_KERNEL_MODE
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PoolLock);
#endif
    }

    Free(buffer, poolTag);
    return newBuffer;
}

void Free(void* buffer, ULONG poolTag) noexcept {
    if (buffer == nullptr) {
        return;
    }

    auto* header = reinterpret_cast<AllocationHeader*>(buffer) - 1;
    if (header->Tag != poolTag) {
        FCL_LOG_WARN("PoolAllocator::Free tag mismatch (expected %lu, got %lu)", poolTag, header->Tag);
    }

    if (g_TrackingEnabled) {
#if FCL_MUSA_KERNEL_MODE
        ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PoolLock);
#else
        std::lock_guard<std::mutex> guard(g_PoolMutex);
#endif
        g_FreeCount++;
        g_BytesFreed += static_cast<long long>(header->Size);
        g_BytesInUse -= static_cast<long long>(header->Size);
#if FCL_MUSA_KERNEL_MODE
        ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PoolLock);
#endif
    }

#if FCL_MUSA_KERNEL_MODE
    ExFreePoolWithTag(header, poolTag);
#else
    std::free(header);
#endif
}

FCL_POOL_STATS QueryStats() noexcept {
    FCL_POOL_STATS stats = {};
    if (g_TrackingEnabled) {
#if FCL_MUSA_KERNEL_MODE
        ExEnterCriticalRegionAndAcquirePushLockShared(&g_PoolLock);
#else
        std::lock_guard<std::mutex> guard(g_PoolMutex);
#endif
        stats.AllocationCount = g_AllocationCount;
        stats.FreeCount = g_FreeCount;
        stats.BytesAllocated = g_BytesAllocated;
        stats.BytesFreed = g_BytesFreed;
        stats.BytesInUse = g_BytesInUse;
        stats.PeakBytesInUse = g_PeakBytesInUse;
#if FCL_MUSA_KERNEL_MODE
        ExReleasePushLockSharedAndLeaveCriticalRegion(&g_PoolLock);
#endif
    }
    return stats;
}

}  // namespace fclmusa::memory
