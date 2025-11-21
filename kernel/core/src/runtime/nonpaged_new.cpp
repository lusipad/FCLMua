#include <ntddk.h>
#include <ntintsafe.h>

#include <new>
#include <cstddef>

#include "fclmusa/memory/pool_allocator.h"
#include "fclmusa/version.h"

#if defined(FCL_MUSA_DPC_NO_DEBUG_CRT)

namespace {

inline size_t NormalizeSize(size_t size) noexcept {
    return (size == 0) ? 1 : size;
}

void* AllocateInternal(size_t size) {
    void* buffer = fclmusa::memory::Allocate(size, FCL_MUSA_DPC_POOL_TAG);
    if (buffer == nullptr) {
        throw std::bad_alloc();
    }
    return buffer;
}

inline void FreeInternal(void* buffer) noexcept {
    fclmusa::memory::Free(buffer, FCL_MUSA_DPC_POOL_TAG);
}

}  // namespace

void* __cdecl operator new(size_t size) {
    return AllocateInternal(NormalizeSize(size));
}

void* __cdecl operator new[](size_t size) {
    return AllocateInternal(NormalizeSize(size));
}

void __cdecl operator delete(void* pointer) noexcept {
    FreeInternal(pointer);
}

void __cdecl operator delete[](void* pointer) noexcept {
    FreeInternal(pointer);
}

void __cdecl operator delete(void* pointer, size_t) noexcept {
    FreeInternal(pointer);
}

void __cdecl operator delete[](void* pointer, size_t) noexcept {
    FreeInternal(pointer);
}

void* __cdecl operator new(size_t size, const std::nothrow_t&) noexcept {
    return fclmusa::memory::Allocate(NormalizeSize(size), FCL_MUSA_DPC_POOL_TAG);
}

void* __cdecl operator new[](size_t size, const std::nothrow_t&) noexcept {
    return fclmusa::memory::Allocate(NormalizeSize(size), FCL_MUSA_DPC_POOL_TAG);
}

void __cdecl operator delete(void* pointer, const std::nothrow_t&) noexcept {
    FreeInternal(pointer);
}

void __cdecl operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    FreeInternal(pointer);
}

void* __cdecl operator new(size_t size, std::align_val_t alignVal) {
    size_t alignment = static_cast<size_t>(alignVal);
    if (alignment <= alignof(void*)) {
        return AllocateInternal(NormalizeSize(size));
    }

    size_t total = 0;
    if (!NT_SUCCESS(RtlSizeTAdd(NormalizeSize(size), alignment, &total))) {
        throw std::bad_alloc();
    }

    void* raw = fclmusa::memory::Allocate(total, FCL_MUSA_DPC_POOL_TAG);
    if (raw == nullptr) {
        throw std::bad_alloc();
    }

    uintptr_t address = reinterpret_cast<uintptr_t>(raw);
    uintptr_t aligned = (address + alignment - 1) & ~(alignment - 1);
    auto** header = reinterpret_cast<void**>(aligned) - 1;
    *header = raw;
    return reinterpret_cast<void*>(aligned);
}

void* __cdecl operator new[](size_t size, std::align_val_t alignVal) {
    return operator new(size, alignVal);
}

void* __cdecl operator new(size_t size, std::align_val_t alignVal, const std::nothrow_t&) noexcept {
    try {
        return operator new(size, alignVal);
    } catch (...) {
        return nullptr;
    }
}

void* __cdecl operator new[](size_t size, std::align_val_t alignVal, const std::nothrow_t& tag) noexcept {
    return operator new(size, alignVal, tag);
}

void __cdecl operator delete(void* pointer, std::align_val_t alignVal) noexcept {
    if (pointer == nullptr) {
        return;
    }

    size_t alignment = static_cast<size_t>(alignVal);
    if (alignment <= alignof(void*)) {
        FreeInternal(pointer);
        return;
    }

    auto** header = reinterpret_cast<void**>(pointer) - 1;
    FreeInternal(*header);
}

void __cdecl operator delete[](void* pointer, std::align_val_t alignVal) noexcept {
    operator delete(pointer, alignVal);
}

void __cdecl operator delete(void* pointer, size_t, std::align_val_t alignVal) noexcept {
    operator delete(pointer, alignVal);
}

void __cdecl operator delete[](void* pointer, size_t, std::align_val_t alignVal) noexcept {
    operator delete(pointer, alignVal);
}

void __cdecl operator delete(void* pointer, std::align_val_t alignVal, const std::nothrow_t&) noexcept {
    operator delete(pointer, alignVal);
}

void __cdecl operator delete[](void* pointer, std::align_val_t alignVal, const std::nothrow_t&) noexcept {
    operator delete(pointer, alignVal);
}

#endif  // FCL_MUSA_DPC_NO_DEBUG_CRT
