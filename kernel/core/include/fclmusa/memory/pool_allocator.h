#pragma once

#include <ntddk.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "fclmusa/version.h"

typedef struct _FCL_POOL_STATS {
    ULONGLONG AllocationCount;
    ULONGLONG FreeCount;
    ULONGLONG BytesAllocated;
    ULONGLONG BytesFreed;
    ULONGLONG BytesInUse;
    ULONGLONG PeakBytesInUse;
} FCL_POOL_STATS, *PFCL_POOL_STATS;

namespace fclmusa::memory {

void InitializePoolTracking();
void ShutdownPoolTracking();

_Must_inspect_result_
void* Allocate(_In_ size_t size, _In_ ULONG poolTag = FCL_MUSA_POOL_TAG) noexcept;

void Free(_Inout_opt_ void* buffer, _In_ ULONG poolTag = FCL_MUSA_POOL_TAG) noexcept;

_Must_inspect_result_
void* Reallocate(_Inout_opt_ void* buffer, _In_ size_t size, _In_ ULONG poolTag = FCL_MUSA_POOL_TAG) noexcept;

size_t QueryAllocationSize(_In_opt_ const void* buffer) noexcept;

FCL_POOL_STATS QueryStats() noexcept;

template <typename T>
struct PoolDeleter {
    void operator()(_Inout_opt_ T* ptr) const noexcept {
        if (ptr != nullptr) {
            ptr->~T();
            Free(ptr, FCL_MUSA_POOL_TAG);
        }
    }
};

template <typename T>
using unique_pool_ptr = std::unique_ptr<T, PoolDeleter<T>>;

}  // namespace fclmusa::memory
