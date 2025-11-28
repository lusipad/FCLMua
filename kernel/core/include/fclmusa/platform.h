#pragma once

// Platform abstraction: allows building both kernel mode (R0) and user mode (R3).
// Default to kernel mode for backward compatibility.
#ifndef FCL_MUSA_KERNEL_MODE
#define FCL_MUSA_KERNEL_MODE 1
#endif

#if FCL_MUSA_KERNEL_MODE

#include <ntddk.h>
#include <wdm.h>
#include <ntintsafe.h>

#ifndef ExEnterCriticalRegionAndAcquirePushLockExclusive
inline VOID ExEnterCriticalRegionAndAcquirePushLockExclusive(PEX_PUSH_LOCK PushLock) {
    KeEnterCriticalRegion();
    ExAcquirePushLockExclusive(PushLock);
}
#endif

#ifndef ExReleasePushLockExclusiveAndLeaveCriticalRegion
inline VOID ExReleasePushLockExclusiveAndLeaveCriticalRegion(PEX_PUSH_LOCK PushLock) {
    ExReleasePushLockExclusive(PushLock);
    KeLeaveCriticalRegion();
}
#endif

#ifndef ExEnterCriticalRegionAndAcquirePushLockShared
inline VOID ExEnterCriticalRegionAndAcquirePushLockShared(PEX_PUSH_LOCK PushLock) {
    KeEnterCriticalRegion();
    ExAcquirePushLockShared(PushLock);
}
#endif

#ifndef ExReleasePushLockSharedAndLeaveCriticalRegion
inline VOID ExReleasePushLockSharedAndLeaveCriticalRegion(PEX_PUSH_LOCK PushLock) {
    ExReleasePushLockShared(PushLock);
    KeLeaveCriticalRegion();
}
#endif

#ifndef NON_PAGED_CODE
#define NON_PAGED_CODE
#endif

#else

#ifndef WIN32_NO_STATUS
#define WIN32_NO_STATUS
#endif
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <sal.h>
#include <intsafe.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <synchapi.h>
#include <winternl.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef EXTERN_C_START
#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif
#endif

#ifndef PAGED_CODE
#define PAGED_CODE()
#endif

#ifndef NON_PAGED_CODE
#define NON_PAGED_CODE
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

// Light-weight kernel compatibility for user mode
typedef unsigned char KIRQL;
#ifndef PASSIVE_LEVEL
#define PASSIVE_LEVEL 0
#endif
inline KIRQL KeGetCurrentIrql() { return PASSIVE_LEVEL; }
inline VOID KeEnterCriticalRegion() {}
inline VOID KeLeaveCriticalRegion() {}

typedef SRWLOCK EX_PUSH_LOCK;
inline VOID ExInitializePushLock(EX_PUSH_LOCK* lock) { InitializeSRWLock(lock); }
inline VOID ExAcquirePushLockExclusive(EX_PUSH_LOCK* lock) { AcquireSRWLockExclusive(lock); }
inline VOID ExReleasePushLockExclusive(EX_PUSH_LOCK* lock) { ReleaseSRWLockExclusive(lock); }
inline VOID ExAcquirePushLockShared(EX_PUSH_LOCK* lock) { AcquireSRWLockShared(lock); }
inline VOID ExReleasePushLockShared(EX_PUSH_LOCK* lock) { ReleaseSRWLockShared(lock); }
inline BOOLEAN ExTryToAcquirePushLockExclusive(EX_PUSH_LOCK* lock) { return TryAcquireSRWLockExclusive(lock); }
inline BOOLEAN ExTryToAcquirePushLockShared(EX_PUSH_LOCK* lock) { return TryAcquireSRWLockShared(lock); }
inline VOID ExfUnblockPushLock(EX_PUSH_LOCK* lock) { UNREFERENCED_PARAMETER(lock); }

static inline LARGE_INTEGER KeQueryPerformanceCounter(_Out_opt_ LARGE_INTEGER* frequency) {
    LARGE_INTEGER counter = {0};
    QueryPerformanceCounter(&counter);
    if (frequency != NULL) {
        QueryPerformanceFrequency(frequency);
    }
    return counter;
}

static inline NTSTATUS RtlSizeTMult(size_t a, size_t b, size_t* out) {
    if (out == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (a != 0 && b > (SIZE_MAX / a)) {
        return STATUS_INTEGER_OVERFLOW;
    }
    *out = a * b;
    return STATUS_SUCCESS;
}

// Logging compatibility levels (used as integers only in user mode)
#ifndef DPFLTR_ERROR_LEVEL
#define DPFLTR_ERROR_LEVEL 0
#endif
#ifndef DPFLTR_WARNING_LEVEL
#define DPFLTR_WARNING_LEVEL 1
#endif
#ifndef DPFLTR_INFO_LEVEL
#define DPFLTR_INFO_LEVEL 2
#endif
#ifndef DPFLTR_TRACE_LEVEL
#define DPFLTR_TRACE_LEVEL 3
#endif
#ifndef DPFLTR_IHVDRIVER_ID
#define DPFLTR_IHVDRIVER_ID 0
#endif

#endif  // FCL_MUSA_KERNEL_MODE
