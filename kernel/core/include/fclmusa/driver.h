#pragma once

#include <ntddk.h>

#include "fclmusa/version.h"
#include "fclmusa/ioctl.h"

EXTERN_C_START

NTSTATUS FclInitialize();
VOID FclCleanup();

NTSTATUS FclQueryHealth(_Out_ FCL_PING_RESPONSE* response);

EXTERN_C_END

