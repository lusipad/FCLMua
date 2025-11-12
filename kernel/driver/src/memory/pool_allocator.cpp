#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <ntddk.h>
#include <ntintsafe.h>
#include <wdm.h>

#include <algorithm>
#include <new>

#include "fclmusa/logging.h"
#include "fclmusa/memory/pool_allocator.h"

// Compatibility helpers for PushLock APIs
#ifndef ExEnterCriticalRegionAndAcquirePushLockExclusive
inline VOID ExEnterCriticalRegionAndAcquirePushLockExclusive(PEX_PUSH_LOCK PushLock) {
    KeEnterCriticalRegion();
    ExAcquirePushLockExclusive(PushLock);
}
#endif

#ifndef ExReleasePushLockExclusiveAndLeaveCriticalRegion
inline VOID ExReleasePushLockExclusiveAndLeaveCriticalRegion(PEX_PUSH_LOCK PushLock) {
    ExReleasePushLockExclusive(PushLock);
    KeLeaveCriticalRegion();
}
#endif

namespace {

struct alignas(void*) AllocationHeader {
    size_t Size;
    ULONG Tag;
};

EX_PUSH_LOCK g_PoolLock = 0;  // Will be initialized in InitializePoolTracking
volatile LONG64 g_AllocationCount = 0;
volatile LONG64 g_FreeCount = 0;
volatile LONG64 g_BytesAllocated = 0;
volatile LONG64 g_BytesFreed = 0;
volatile LONG64 g_BytesInUse = 0;
volatile LONG64 g_PeakBytesInUse = 0;
BOOLEAN g_TrackingEnabled = FALSE;

void ResetStatsUnsafe() {
    g_AllocationCount = 0;
    g_FreeCount = 0;
    g_BytesAllocated = 0;
    g_BytesFreed = 0;
    g_BytesInUse = 0;
    g_PeakBytesInUse = 0;
}

void UpdatePeak(LONGLONG inUse) {
    LONGLONG previous = g_PeakBytesInUse;
    while (inUse > previous) {
        const LONGLONG observed = InterlockedCompareExchange64(&g_PeakBytesInUse, inUse, previous);
        if (observed == previous) {
            break;
        }
        previous = observed;
    }
}

size_t RequestedSizeWithHeader(size_t payload) {
    size_t total = 0;
    if (!NT_SUCCESS(RtlSizeTAdd(payload, sizeof(AllocationHeader), &total))) {
        return 0;
    }
    return total;
}

}  // namespace

namespace fclmusa::memory {

void InitializePoolTracking() {
    ExInitializePushLock(&g_PoolLock);
    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PoolLock);
    ResetStatsUnsafe();
    g_TrackingEnabled = TRUE;
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PoolLock);
}

void ShutdownPoolTracking() {
    ExEnterCriticalRegionAndAcquirePushLockExclusive(&g_PoolLock);
    g_TrackingEnabled = FALSE;
    ExReleasePushLockExclusiveAndLeaveCriticalRegion(&g_PoolLock);
}

void* Allocate(size_t size, ULONG poolTag) noexcept {
    if (size == 0) {
        size = 1;
    }

    const size_t totalSize = RequestedSizeWithHeader(size);
    if (totalSize == 0) {
        return nullptr;
    }

#if (NTDDI_VERSION >= NTDDI_WIN8)
    void* raw = ExAllocatePool2(POOL_FLAG_NON_PAGED, totalSize, poolTag);
#else
    void* raw = ExAllocatePoolWithTag(NonPagedPoolNx, totalSize, poolTag);
#endif

    if (raw == nullptr) {
        return nullptr;
    }

    auto* header = reinterpret_cast<AllocationHeader*>(raw);
    header->Size = size;
    header->Tag = poolTag;

    if (g_TrackingEnabled) {
        InterlockedIncrement64(&g_AllocationCount);
        InterlockedAdd64(&g_BytesAllocated, static_cast<LONGLONG>(size));
        const auto inUse = InterlockedAdd64(&g_BytesInUse, static_cast<LONGLONG>(size));
        UpdatePeak(inUse);
    }

    return header + 1;
}

void Free(void* buffer, ULONG poolTag) noexcept {
    if (buffer == nullptr) {
        return;
    }

    auto* header = reinterpret_cast<AllocationHeader*>(buffer) - 1;
    const size_t size = header->Size;
    const ULONG tag = header->Tag;

    if (g_TrackingEnabled) {
        InterlockedIncrement64(&g_FreeCount);
        InterlockedAdd64(&g_BytesFreed, static_cast<LONGLONG>(size));
        InterlockedAdd64(&g_BytesInUse, -static_cast<LONGLONG>(size));
    }

    UNREFERENCED_PARAMETER(poolTag);
    ExFreePoolWithTag(header, tag);
}

FCL_POOL_STATS QueryStats() noexcept {
    FCL_POOL_STATS stats = {};
    stats.AllocationCount = static_cast<ULONGLONG>(g_AllocationCount);
    stats.FreeCount = static_cast<ULONGLONG>(g_FreeCount);
    stats.BytesAllocated = static_cast<ULONGLONG>(g_BytesAllocated);
    stats.BytesFreed = static_cast<ULONGLONG>(g_BytesFreed);
    stats.BytesInUse = static_cast<ULONGLONG>(g_BytesInUse);
    stats.PeakBytesInUse = static_cast<ULONGLONG>(g_PeakBytesInUse);
    return stats;
}

void* Reallocate(void* buffer, size_t size, ULONG poolTag) noexcept {
    if (buffer == nullptr) {
        return Allocate(size, poolTag);
    }

    if (size == 0) {
        Free(buffer, poolTag);
        return nullptr;
    }

    const size_t oldSize = QueryAllocationSize(buffer);
    void* newBuffer = Allocate(size, poolTag);
    if (newBuffer == nullptr) {
        return nullptr;
    }

    const size_t bytesToCopy = std::min(oldSize, size);
    RtlCopyMemory(newBuffer, buffer, bytesToCopy);
    Free(buffer, poolTag);
    return newBuffer;
}

size_t QueryAllocationSize(const void* buffer) noexcept {
    if (buffer == nullptr) {
        return 0;
    }
    const auto* header = reinterpret_cast<const AllocationHeader*>(buffer) - 1;
    return header->Size;
}

}  // namespace fclmusa::memory

void* __cdecl operator new(size_t size) {
    void* buffer = fclmusa::memory::Allocate(size);
    if (buffer == nullptr) {
        ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
    }
    return buffer;
}

void* __cdecl operator new[](size_t size) {
    return operator new(size);
}

void* __cdecl operator new(size_t size, const std::nothrow_t&) noexcept {
    return fclmusa::memory::Allocate(size);
}

void* __cdecl operator new[](size_t size, const std::nothrow_t&) noexcept {
    return fclmusa::memory::Allocate(size);
}

void __cdecl operator delete(void* ptr) noexcept {
    fclmusa::memory::Free(ptr);
}

void __cdecl operator delete[](void* ptr) noexcept {
    fclmusa::memory::Free(ptr);
}

void __cdecl operator delete(void* ptr, size_t) noexcept {
    fclmusa::memory::Free(ptr);
}

void __cdecl operator delete[](void* ptr, size_t) noexcept {
    fclmusa::memory::Free(ptr);
}
