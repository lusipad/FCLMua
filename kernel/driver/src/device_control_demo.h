#pragma once

#include <ntddk.h>

#ifdef FCL_MUSA_ENABLE_DEMO

NTSTATUS
HandleSphereCollisionDemo(
    _Inout_ PIRP irp,
    _In_ PIO_STACK_LOCATION stack);

#endif

