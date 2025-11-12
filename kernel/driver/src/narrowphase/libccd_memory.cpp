#include "fclmusa/narrowphase/libccd_memory.h"

#include <ntddk.h>
#include <ntintsafe.h>

#include "fclmusa/memory/pool_allocator.h"

extern "C"
void* FclCcdRealloc(_Inout_opt_ void* buffer, _In_ size_t size) {
    return fclmusa::memory::Reallocate(buffer, size);
}

extern "C"
void* FclCcdCalloc(_In_ size_t count, _In_ size_t size) {
    size_t total = 0;
    if (!NT_SUCCESS(RtlSizeTMult(count, size, &total))) {
        return nullptr;
    }
    void* ptr = fclmusa::memory::Allocate(total);
    if (ptr != nullptr) {
        RtlZeroMemory(ptr, total);
    }
    return ptr;
}

extern "C"
void FclCcdFree(_In_opt_ void* buffer) {
    fclmusa::memory::Free(buffer);
}
