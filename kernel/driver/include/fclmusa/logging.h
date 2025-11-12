#pragma once

#include <ntddk.h>

#define FCL_MUSA_LOG_PREFIX "[FCL+Musa] "

#define FCL_LOG_LEVEL_ERROR DPFLTR_ERROR_LEVEL
#define FCL_LOG_LEVEL_WARN  DPFLTR_WARNING_LEVEL
#define FCL_LOG_LEVEL_INFO  DPFLTR_INFO_LEVEL
#define FCL_LOG_LEVEL_TRACE DPFLTR_TRACE_LEVEL

#define FCL_LOG_PRINT(level, fmt, ...)                                                          \
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, level, FCL_MUSA_LOG_PREFIX fmt "\n", __VA_ARGS__)

#define FCL_LOG_ERROR(fmt, ...) FCL_LOG_PRINT(FCL_LOG_LEVEL_ERROR, fmt, __VA_ARGS__)
#define FCL_LOG_WARN(fmt, ...)  FCL_LOG_PRINT(FCL_LOG_LEVEL_WARN, fmt, __VA_ARGS__)
#define FCL_LOG_INFO(fmt, ...)  FCL_LOG_PRINT(FCL_LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#define FCL_LOG_TRACE(fmt, ...) FCL_LOG_PRINT(FCL_LOG_LEVEL_TRACE, fmt, __VA_ARGS__)

#define FCL_LOG_ERROR0(msg) FCL_LOG_ERROR("%s", msg)
#define FCL_LOG_WARN0(msg)  FCL_LOG_WARN("%s", msg)
#define FCL_LOG_INFO0(msg)  FCL_LOG_INFO("%s", msg)
#define FCL_LOG_TRACE0(msg) FCL_LOG_TRACE("%s", msg)

