#pragma once

#include "fclmusa/platform.h"

#include <cstddef>

#if FCL_MUSA_KERNEL_MODE
    #include <kmalloc.h>  // Musa.Runtime
#endif

#include "fclmusa/version.h"

namespace fclmusa::memory {

/// @brief DPC-safe allocator for STL containers in kernel mode.
///
/// This allocator uses Musa.Runtime's kmalloc/kfree which are safe to call
/// at DISPATCH_LEVEL (DPC context). It allocates from NonPagedPoolNx.
///
/// IMPORTANT: This allocator returns nullptr on failure instead of throwing
/// std::bad_alloc, because C++ exceptions are not supported at DISPATCH_LEVEL.
/// Callers must check for nullptr after container operations.
///
/// @tparam T The type to allocate
template <typename T>
class FclDpcNonPagedAllocator {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    FclDpcNonPagedAllocator() noexcept = default;

    template <typename U>
    FclDpcNonPagedAllocator(const FclDpcNonPagedAllocator<U>&) noexcept {}

    /// @brief Allocates memory for count objects of type T.
    /// @param count Number of objects to allocate space for.
    /// @return Pointer to allocated memory, or nullptr on failure.
    /// @note Does NOT throw exceptions - safe for DPC context.
    [[nodiscard]] T* allocate(size_t count) noexcept {
#if FCL_MUSA_KERNEL_MODE
        size_t total = 0;
        if (!NT_SUCCESS(RtlSizeTMult(count, sizeof(T), &total))) {
            return nullptr;  // Overflow - return nullptr instead of throwing
        }

        // Use Musa.Runtime kmalloc - DPC safe, no exceptions
        return static_cast<T*>(kmalloc(total, NonPagedPoolNx, FCL_MUSA_DPC_POOL_TAG));
#else
        // User-mode fallback
        return static_cast<T*>(std::malloc(count * sizeof(T)));
#endif
    }

    /// @brief Deallocates memory previously allocated by this allocator.
    /// @param ptr Pointer to memory to deallocate.
    /// @param count Number of objects (unused, but required by allocator interface).
    void deallocate(T* ptr, size_t /*count*/) noexcept {
        if (ptr == nullptr) {
            return;
        }
#if FCL_MUSA_KERNEL_MODE
        // Use Musa.Runtime kfree - DPC safe
        kfree(ptr, FCL_MUSA_DPC_POOL_TAG);
#else
        std::free(ptr);
#endif
    }

    /// @brief Maximum number of objects that can be allocated.
    [[nodiscard]] size_type max_size() const noexcept {
        return static_cast<size_type>(-1) / sizeof(T);
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
