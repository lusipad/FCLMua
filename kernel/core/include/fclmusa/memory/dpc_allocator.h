#pragma once

#include <ntddk.h>
#include <ntintsafe.h>

#include <cstddef>
#include <new>

#include "fclmusa/memory/pool_allocator.h"
#include "fclmusa/version.h"

namespace fclmusa::memory {

template <typename T>
class FclDpcNonPagedAllocator {
public:
    using value_type = T;

    FclDpcNonPagedAllocator() noexcept = default;

    template <typename U>
    FclDpcNonPagedAllocator(const FclDpcNonPagedAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(size_t count) {
        size_t total = 0;
        if (!NT_SUCCESS(RtlSizeTMult(count, sizeof(T), &total))) {
            throw std::bad_alloc();
        }

        void* buffer = fclmusa::memory::Allocate(total, FCL_MUSA_DPC_POOL_TAG);
        if (buffer == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(buffer);
    }

    void deallocate(T* ptr, size_t) noexcept {
        fclmusa::memory::Free(ptr, FCL_MUSA_DPC_POOL_TAG);
    }
};

template <typename T, typename U>
inline bool operator==(const FclDpcNonPagedAllocator<T>&, const FclDpcNonPagedAllocator<U>&) noexcept {
    return true;
}

template <typename T, typename U>
inline bool operator!=(const FclDpcNonPagedAllocator<T>& lhs, const FclDpcNonPagedAllocator<U>& rhs) noexcept {
    return !(lhs == rhs);
}

}  // namespace fclmusa::memory
