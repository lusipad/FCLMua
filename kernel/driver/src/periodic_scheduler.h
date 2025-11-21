#pragma once

#include <ntddk.h>

//
// 通用的周期调度器：
// - 使用 KTIMER + KDPC 提供精确周期节拍（DPC，DISPATCH_LEVEL）
// - 使用内核线程在 PASSIVE_LEVEL 执行用户提供的回调
// - 设计目标：最小化 DPC 工作量，把所有重计算放到 PASSIVE_LEVEL
//

EXTERN_C_START

typedef
_IRQL_requires_(PASSIVE_LEVEL)
VOID
(*PFCL_PERIODIC_WORK_CALLBACK)(
    _In_opt_ PVOID Context
    ) noexcept;

//
// 初始化并启动周期调度器。
//
// 参数:
//   callback         - 每次周期触发时在 PASSIVE_LEVEL 执行的回调
//   callbackContext  - 传递给回调的上下文指针
//   periodMicrosecs  - 周期（微秒），最小粒度约为 1ms
//
// 约束:
//   - 必须在 PASSIVE_LEVEL 调用
//   - 调用一次成功后，如需修改周期/回调，应先调用 FclPeriodicSchedulerShutdown
//
NTSTATUS
FclPeriodicSchedulerInitialize(
    _In_ PFCL_PERIODIC_WORK_CALLBACK callback,
    _In_opt_ PVOID callbackContext,
    _In_ ULONG periodMicrosecs
    ) noexcept;

//
// 停止调度器并终止内部工作线程。
// - 将等待当前工作线程安全退出，适合在 DriverUnload 时调用。
//
_IRQL_requires_(PASSIVE_LEVEL)
VOID
FclPeriodicSchedulerShutdown() noexcept;

EXTERN_C_END

