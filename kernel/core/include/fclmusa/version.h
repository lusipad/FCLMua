#pragma once

#include <ntddk.h>

#define FCL_MUSA_DRIVER_VERSION_MAJOR 0u
#define FCL_MUSA_DRIVER_VERSION_MINOR 1u
#define FCL_MUSA_DRIVER_VERSION_PATCH 0u
#define FCL_MUSA_DRIVER_VERSION_BUILD 1u

#define FCL_MUSA_DRIVER_NAME            L"FCL+Musa"
#define FCL_MUSA_DEVICE_NAME            L"\\Device\\FclMusa"
#define FCL_MUSA_DOS_DEVICE_NAME        L"\\DosDevices\\FclMusa"

#define FCL_MUSA_POOL_TAG ' LCF'

typedef struct _FCL_DRIVER_VERSION {
    ULONG Major;
    ULONG Minor;
    ULONG Patch;
    ULONG Build;
} FCL_DRIVER_VERSION;

EXTERN_C const FCL_DRIVER_VERSION* FclGetDriverVersion();

