#pragma once

#include "fclmusa/platform.h"

EXTERN_C_START

void* FclCcdRealloc(_Inout_opt_ void* buffer, _In_ size_t size);
void* FclCcdCalloc(_In_ size_t count, _In_ size_t size);
void FclCcdFree(_In_opt_ void* buffer);

EXTERN_C_END
