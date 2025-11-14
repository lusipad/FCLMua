#pragma once

#include <ntddk.h>

#include "fclmusa/ioctl.h"

EXTERN_C_START

NTSTATUS
FclRunSelfTest(
    _Out_ PFCL_SELF_TEST_RESULT result) noexcept;

EXTERN_C_END

