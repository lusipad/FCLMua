#pragma once

#include <stddef.h>

#include "fclmusa/narrowphase/libccd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define __CCD_ALLOC_MEMORY(type, ptr_old, size) \
    (type*)FclCcdRealloc((void*)(ptr_old), (size))

#define CCD_ALLOC(type) \
    __CCD_ALLOC_MEMORY(type, NULL, sizeof(type))

#define CCD_ALLOC_ARR(type, num_elements) \
    __CCD_ALLOC_MEMORY(type, NULL, sizeof(type) * (num_elements))

#define CCD_REALLOC_ARR(ptr, type, num_elements) \
    __CCD_ALLOC_MEMORY(type, ptr, sizeof(type) * (num_elements))

#define CCD_CALLOC(type, num_elements) \
    (type*)FclCcdCalloc(num_elements, sizeof(type))

#define CCD_FREE(ptr) \
    FclCcdFree(ptr)

#ifdef __cplusplus
}
#endif
