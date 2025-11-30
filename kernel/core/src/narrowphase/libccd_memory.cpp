#include "fclmusa/narrowphase/libccd_memory.h"

#include "fclmusa/platform.h"
#if !FCL_MUSA_KERNEL_MODE
    #include <limits>
#endif

#include "fclmusa/memory/pool_allocator.h"

extern "C"
void* FclCcdRealloc(_Inout_opt_ void* buffer, _In_ size_t size) {
    return fclmusa::memory::Reallocate(buffer, size);
}

namespace {

inline bool SafeSizeMult(size_t a, size_t b, size_t* out) {
#if FCL_MUSA_KERNEL_MODE
    return NT_SUCCESS(RtlSizeTMult(a, b, out));
#else
    if (a != 0 && b > (std::numeric_limits<size_t>::max)() / a) {
        return false;
    }
    *out = a * b;
    return true;
#endif
}

}  // namespace

extern "C"
void* FclCcdCalloc(_In_ size_t count, _In_ size_t size) {
    size_t total = 0;
    if (!SafeSizeMult(count, size, &total)) {
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
