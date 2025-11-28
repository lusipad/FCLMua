#pragma once

#include "fclmusa/platform.h"

#include "fclmusa/version.h"
#include "fclmusa/ioctl.h"

EXTERN_C_START

#if FCL_MUSA_KERNEL_MODE

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

#else  // FCL_MUSA_KERNEL_MODE == 0 (user-mode stubs)

static inline NTSTATUS FclInitialize() { return STATUS_NOT_SUPPORTED; }
static inline VOID FclCleanup() {}

static inline NTSTATUS FclQueryHealth(_Out_ FCL_PING_RESPONSE* /*response*/) {
    return STATUS_NOT_SUPPORTED;
}

static inline VOID
FclDiagnosticsRecordCollisionDuration(_In_ ULONGLONG /*durationMicroseconds*/) noexcept {}

static inline VOID
FclDiagnosticsRecordDpcCollisionDuration(_In_ ULONGLONG /*durationMicroseconds*/) noexcept {}

static inline VOID
FclDiagnosticsRecordDistanceDuration(_In_ ULONGLONG /*durationMicroseconds*/) noexcept {}

static inline VOID
FclDiagnosticsRecordContinuousCollisionDuration(_In_ ULONGLONG /*durationMicroseconds*/) noexcept {}

static inline NTSTATUS
FclQueryDiagnostics(_Out_ FCL_DIAGNOSTICS_RESPONSE* /*response*/) noexcept {
    return STATUS_NOT_SUPPORTED;
}

#endif  // FCL_MUSA_KERNEL_MODE

EXTERN_C_END
