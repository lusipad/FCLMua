#pragma once

#include <ntddk.h>

EXTERN_C_START

NTSTATUS FclRunEigenSmokeTest() noexcept;

NTSTATUS FclRunEigenExtendedTest() noexcept;

EXTERN_C_END

