#pragma once

#include <ntddk.h>

#include "fclmusa/version.h"
#include "fclmusa/ioctl.h"

EXTERN_C_START

NTSTATUS FclInitialize();
VOID FclCleanup();

NTSTATUS FclQueryHealth(_Out_ FCL_PING_RESPONSE* response);

VOID
FclDiagnosticsRecordCollisionDuration(_In_ ULONGLONG durationMicroseconds) noexcept;

VOID
FclDiagnosticsRecordDpcCollisionDuration(_In_ ULONGLONG durationMicroseconds) noexcept;

VOID
FclDiagnosticsRecordDistanceDuration(_In_ ULONGLONG durationMicroseconds) noexcept;

VOID
FclDiagnosticsRecordContinuousCollisionDuration(_In_ ULONGLONG durationMicroseconds) noexcept;

NTSTATUS
FclQueryDiagnostics(_Out_ FCL_DIAGNOSTICS_RESPONSE* response) noexcept;

EXTERN_C_END
