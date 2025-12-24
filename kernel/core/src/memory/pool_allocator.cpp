#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fclmusa/platform.h"

#if FCL_MUSA_KERNEL_MODE
    #include <kmalloc.h>  // Musa.Runtime
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

// Allocation header to track size for statistics
// Aligned to pointer size for proper memory alignment
struct alignas(void*) AllocationHeader {
    size_t Size;
    ULONG Tag;
};

#if FCL_MUSA_KERNEL_MODE
// Use atomic operations instead of Push Lock for DPC safety
volatile LONG64 g_AllocationCount = 0;
volatile LONG64 g_FreeCount = 0;
volatile LONG64 g_BytesAllocated = 0;
volatile LONG64 g_BytesFreed = 0;
volatile LONG64 g_BytesInUse = 0;
volatile LONG64 g_PeakBytesInUse = 0;
volatile LONG g_TrackingEnabled = FALSE;
#else
std::atomic<long long> g_AllocationCount{0};
std::atomic<long long> g_FreeCount{0};
std::atomic<long long> g_BytesAllocated{0};
std::atomic<long long> g_BytesFreed{0};
std::atomic<long long> g_BytesInUse{0};
std::atomic<long long> g_PeakBytesInUse{0};
std::atomic<bool> g_TrackingEnabled{false};
#endif

void ResetStatsUnsafe() {
#if FCL_MUSA_KERNEL_MODE
    InterlockedExchange64(&g_AllocationCount, 0);
    InterlockedExchange64(&g_FreeCount, 0);
    InterlockedExchange64(&g_BytesAllocated, 0);
    InterlockedExchange64(&g_BytesFreed, 0);
    InterlockedExchange64(&g_BytesInUse, 0);
    InterlockedExchange64(&g_PeakBytesInUse, 0);
#else
    g_AllocationCount = 0;
    g_FreeCount = 0;
    g_BytesAllocated = 0;
    g_BytesFreed = 0;
    g_BytesInUse = 0;
    g_PeakBytesInUse = 0;
#endif
}

void UpdatePeak(long long inUse) {
#if FCL_MUSA_KERNEL_MODE
    LONG64 previous = g_PeakBytesInUse;
    while (inUse > previous) {
        const LONG64 observed = InterlockedCompareExchange64(&g_PeakBytesInUse, inUse, previous);
        if (observed == previous) {
            break;
        }
        previous = observed;
    }
#else
    long long expected = g_PeakBytesInUse.load(std::memory_order_relaxed);
    while (inUse > expected) {
        if (g_PeakBytesInUse.compare_exchange_weak(expected, inUse, std::memory_order_relaxed)) {
            break;
        }
    }
#endif
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

inline bool IsTrackingEnabled() {
#if FCL_MUSA_KERNEL_MODE
    return InterlockedCompareExchange(&g_TrackingEnabled, 0, 0) != FALSE;
#else
    return g_TrackingEnabled.load(std::memory_order_relaxed);
#endif
}

}  // namespace

namespace fclmusa::memory {

void InitializePoolTracking() {
    ResetStatsUnsafe();
#if FCL_MUSA_KERNEL_MODE
    InterlockedExchange(&g_TrackingEnabled, TRUE);
#else
    g_TrackingEnabled.store(true, std::memory_order_release);
#endif
}

void ShutdownPoolTracking() {
#if FCL_MUSA_KERNEL_MODE
    InterlockedExchange(&g_TrackingEnabled, FALSE);
#else
    g_TrackingEnabled.store(false, std::memory_order_release);
#endif
    ResetStatsUnsafe();
}

void EnablePoolTracking(BOOLEAN enable) {
#if FCL_MUSA_KERNEL_MODE
    InterlockedExchange(&g_TrackingEnabled, enable ? TRUE : FALSE);
#else
    g_TrackingEnabled.store(enable != FALSE, std::memory_order_release);
#endif
}

void* Allocate(size_t size, ULONG poolTag) noexcept {
    NON_PAGED_CODE;

    const size_t totalSize = RequestedSizeWithHeader(size);
    if (totalSize == 0) {
        return nullptr;
    }

    void* raw = nullptr;
#if FCL_MUSA_KERNEL_MODE
    // Use Musa.Runtime kmalloc - DPC safe, no exceptions, no locks
    raw = kmalloc(totalSize, NonPagedPoolNx, poolTag);
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

    if (IsTrackingEnabled()) {
        // Lock-free atomic operations - DPC safe
#if FCL_MUSA_KERNEL_MODE
        InterlockedIncrement64(&g_AllocationCount);
        InterlockedAdd64(&g_BytesAllocated, static_cast<LONG64>(size));
        LONG64 inUse = InterlockedAdd64(&g_BytesInUse, static_cast<LONG64>(size));
        UpdatePeak(inUse);
#else
        g_AllocationCount.fetch_add(1, std::memory_order_relaxed);
        g_BytesAllocated.fetch_add(static_cast<long long>(size), std::memory_order_relaxed);
        long long inUse = g_BytesInUse.fetch_add(static_cast<long long>(size), std::memory_order_relaxed)
                          + static_cast<long long>(size);
        UpdatePeak(inUse);
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

    auto* oldHeader = reinterpret_cast<AllocationHeader*>(buffer) - 1;
    const size_t oldSize = oldHeader->Size;
    const size_t bytesToCopy = (std::min)(size, oldSize);

    void* raw = nullptr;
#if FCL_MUSA_KERNEL_MODE
    // Use Musa.Runtime kmalloc - DPC safe
    raw = kmalloc(totalSize, NonPagedPoolNx, poolTag);
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

    if (IsTrackingEnabled()) {
        // Lock-free atomic operations - DPC safe
#if FCL_MUSA_KERNEL_MODE
        InterlockedIncrement64(&g_AllocationCount);
        InterlockedAdd64(&g_BytesAllocated, static_cast<LONG64>(size));
        LONG64 inUse = InterlockedAdd64(&g_BytesInUse, static_cast<LONG64>(size));
        UpdatePeak(inUse);
#else
        g_AllocationCount.fetch_add(1, std::memory_order_relaxed);
        g_BytesAllocated.fetch_add(static_cast<long long>(size), std::memory_order_relaxed);
        long long inUse = g_BytesInUse.fetch_add(static_cast<long long>(size), std::memory_order_relaxed)
                          + static_cast<long long>(size);
        UpdatePeak(inUse);
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
    const size_t size = header->Size;

    if (header->Tag != poolTag) {
        FCL_LOG_WARN("PoolAllocator::Free tag mismatch (expected %lu, got %lu)", poolTag, header->Tag);
    }

    if (IsTrackingEnabled()) {
        // Lock-free atomic operations - DPC safe
#if FCL_MUSA_KERNEL_MODE
        InterlockedIncrement64(&g_FreeCount);
        InterlockedAdd64(&g_BytesFreed, static_cast<LONG64>(size));
        InterlockedAdd64(&g_BytesInUse, -static_cast<LONG64>(size));
#else
        g_FreeCount.fetch_add(1, std::memory_order_relaxed);
        g_BytesFreed.fetch_add(static_cast<long long>(size), std::memory_order_relaxed);
        g_BytesInUse.fetch_sub(static_cast<long long>(size), std::memory_order_relaxed);
#endif
    }

#if FCL_MUSA_KERNEL_MODE
    // Use Musa.Runtime kfree - DPC safe
    kfree(header, poolTag);
#else
    std::free(header);
#endif
}

FCL_POOL_STATS QueryStats() noexcept {
    FCL_POOL_STATS stats = {};
    if (IsTrackingEnabled()) {
        // Lock-free reads - DPC safe
#if FCL_MUSA_KERNEL_MODE
        stats.AllocationCount = static_cast<ULONGLONG>(InterlockedCompareExchange64(&g_AllocationCount, 0, 0));
        stats.FreeCount = static_cast<ULONGLONG>(InterlockedCompareExchange64(&g_FreeCount, 0, 0));
        stats.BytesAllocated = static_cast<ULONGLONG>(InterlockedCompareExchange64(&g_BytesAllocated, 0, 0));
        stats.BytesFreed = static_cast<ULONGLONG>(InterlockedCompareExchange64(&g_BytesFreed, 0, 0));
        stats.BytesInUse = static_cast<ULONGLONG>(InterlockedCompareExchange64(&g_BytesInUse, 0, 0));
        stats.PeakBytesInUse = static_cast<ULONGLONG>(InterlockedCompareExchange64(&g_PeakBytesInUse, 0, 0));
#else
        stats.AllocationCount = static_cast<ULONGLONG>(g_AllocationCount.load(std::memory_order_relaxed));
        stats.FreeCount = static_cast<ULONGLONG>(g_FreeCount.load(std::memory_order_relaxed));
        stats.BytesAllocated = static_cast<ULONGLONG>(g_BytesAllocated.load(std::memory_order_relaxed));
        stats.BytesFreed = static_cast<ULONGLONG>(g_BytesFreed.load(std::memory_order_relaxed));
        stats.BytesInUse = static_cast<ULONGLONG>(g_BytesInUse.load(std::memory_order_relaxed));
        stats.PeakBytesInUse = static_cast<ULONGLONG>(g_PeakBytesInUse.load(std::memory_order_relaxed));
#endif
    }
    return stats;
}

}  // namespace fclmusa::memory
