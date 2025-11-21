#include "periodic_scheduler.h"

//
// 实现说明：
//
// - DPC 只负责：根据 KTIMER 周期唤醒内部工作线程（KeSetEvent）
// - 内部工作线程在 PASSIVE_LEVEL 睡眠，按周期被唤醒后调用用户回调
// - 这样既利用 DPC 提供较稳定的周期节拍，又保持 FCL 之类逻辑在 PASSIVE_LEVEL 执行
//

namespace {

struct FCL_PERIODIC_SCHEDULER {
    KTIMER Timer;
    KDPC TimerDpc;
    KEVENT WorkerEvent;
    KEVENT StopEvent;
    KEVENT ThreadExitEvent;

    HANDLE WorkerThreadHandle;

    PFCL_PERIODIC_WORK_CALLBACK Callback;
    PVOID CallbackContext;

    volatile LONG Initialized;
    volatile LONG Running;
    ULONG PeriodMicrosecs;
};

FCL_PERIODIC_SCHEDULER g_Scheduler = {};

_Function_class_(KDEFERRED_ROUTINE)
_IRQL_requires_(DISPATCH_LEVEL)
VOID
NTAPI
FclPeriodicTimerDpc(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    ) {
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    auto* scheduler = reinterpret_cast<FCL_PERIODIC_SCHEDULER*>(DeferredContext);
    if (scheduler == nullptr) {
        return;
    }

    if (scheduler->Running == 0) {
        return;
    }

    //
    // DPC 仅唤醒工作线程，不在此执行耗时操作。
    //
    KeSetEvent(&scheduler->WorkerEvent, IO_NO_INCREMENT, FALSE);
}

VOID
FclPeriodicWorkerThread(
    _In_ PVOID StartContext
    ) {
    auto* scheduler = reinterpret_cast<FCL_PERIODIC_SCHEDULER*>(StartContext);
    if (scheduler == nullptr) {
        PsTerminateSystemThread(STATUS_INVALID_PARAMETER);
    }

    for (;;) {
        PKEVENT events[2] = {
            &scheduler->StopEvent,
            &scheduler->WorkerEvent,
        };

        NTSTATUS waitStatus = KeWaitForMultipleObjects(
            RTL_NUMBER_OF(events),
            reinterpret_cast<PVOID*>(events),
            WaitAny,
            Executive,
            KernelMode,
            FALSE,
            nullptr,
            nullptr);

        if (!NT_SUCCESS(waitStatus)) {
            continue;
        }

        if (waitStatus == STATUS_WAIT_0) {
            //
            // StopEvent 触发，退出线程。
            //
            break;
        }

        if (waitStatus == STATUS_WAIT_1) {
            //
            // WorkerEvent 触发，执行回调。
            //
            PFCL_PERIODIC_WORK_CALLBACK callback = scheduler->Callback;
            PVOID context = scheduler->CallbackContext;
            if (callback != nullptr && scheduler->Running != 0) {
                callback(context);
            }
        }
    }

    //
    // 通知 Shutdown 调用方：工作线程已经完全退出。
    //
    KeSetEvent(&scheduler->ThreadExitEvent, IO_NO_INCREMENT, FALSE);

    PsTerminateSystemThread(STATUS_SUCCESS);
}

}  // namespace

NTSTATUS
FclPeriodicSchedulerInitialize(
    _In_ PFCL_PERIODIC_WORK_CALLBACK callback,
    _In_opt_ PVOID callbackContext,
    _In_ ULONG periodMicrosecs
    ) noexcept {
    if (callback == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (periodMicrosecs == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // 只允许初始化一次，如需重新配置需先 Shutdown。
    //
    LONG expected = 0;
    if (InterlockedCompareExchange(&g_Scheduler.Initialized, 1, expected) != expected) {
        return STATUS_ALREADY_INITIALIZED;
    }

    RtlZeroMemory(&g_Scheduler, sizeof(g_Scheduler));

    g_Scheduler.Callback = callback;
    g_Scheduler.CallbackContext = callbackContext;
    g_Scheduler.PeriodMicrosecs = periodMicrosecs;
    g_Scheduler.Running = 1;

    KeInitializeTimerEx(&g_Scheduler.Timer, NotificationTimer);
    KeInitializeDpc(&g_Scheduler.TimerDpc, FclPeriodicTimerDpc, &g_Scheduler);

    KeInitializeEvent(&g_Scheduler.WorkerEvent, SynchronizationEvent, FALSE);
    KeInitializeEvent(&g_Scheduler.StopEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&g_Scheduler.ThreadExitEvent, NotificationEvent, FALSE);

    HANDLE threadHandle = nullptr;
    NTSTATUS status = PsCreateSystemThread(
        &threadHandle,
        THREAD_ALL_ACCESS,
        nullptr,
        nullptr,
        nullptr,
        FclPeriodicWorkerThread,
        &g_Scheduler);

    if (!NT_SUCCESS(status)) {
        g_Scheduler.Running = 0;
        g_Scheduler.Initialized = 0;
        return status;
    }

    g_Scheduler.WorkerThreadHandle = threadHandle;
    g_Scheduler.Initialized = 1;

    //
    // 设置周期定时器：
    // - 首次触发延迟一个周期
    // - 周期单位为毫秒，内核定时器最小粒度依赖于系统时钟
    //
    const ULONG periodMs = (periodMicrosecs + 999) / 1000;
    if (periodMs == 0) {
        //
        // 最小 1ms
        //
        const LONG minPeriodMs = 1;
        LARGE_INTEGER dueTime = {};
        dueTime.QuadPart = -10LL * static_cast<LONGLONG>(minPeriodMs) * 1000LL;
        KeSetTimerEx(
            &g_Scheduler.Timer,
            dueTime,
            minPeriodMs,
            &g_Scheduler.TimerDpc);
    } else {
        LARGE_INTEGER dueTime = {};
        dueTime.QuadPart = -10LL * static_cast<LONGLONG>(periodMs) * 1000LL;
        KeSetTimerEx(
            &g_Scheduler.Timer,
            dueTime,
            static_cast<LONG>(periodMs),
            &g_Scheduler.TimerDpc);
    }

    return STATUS_SUCCESS;
}

VOID
FclPeriodicSchedulerShutdown() noexcept {
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return;
    }

    if (g_Scheduler.Running == 0) {
        return;
    }

    g_Scheduler.Running = 0;

    //
    // 停止定时器并通知工作线程退出。
    //
    KeCancelTimer(&g_Scheduler.Timer);
    KeSetEvent(&g_Scheduler.StopEvent, IO_NO_INCREMENT, FALSE);

    //
    // 等待工作线程退出信号，避免驱动卸载时仍有线程执行。
    //
    KeWaitForSingleObject(
        &g_Scheduler.ThreadExitEvent,
        Executive,
        KernelMode,
        FALSE,
        nullptr);

    if (g_Scheduler.WorkerThreadHandle != nullptr) {
        ZwClose(g_Scheduler.WorkerThreadHandle);
        g_Scheduler.WorkerThreadHandle = nullptr;
    }

    g_Scheduler.Initialized = 0;
}
