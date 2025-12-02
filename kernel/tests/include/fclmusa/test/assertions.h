#pragma once

#include <ntddk.h>
#include <cmath>

#include "fclmusa/logging.h"

#define FCL_TEST_EXPECT_TRUE(expr, status) \
    do { \
        if (!(expr)) { \
            FCL_LOG_ERROR("[TEST] EXPECT_TRUE failed: %s", #expr); \
            return (status); \
        } \
    } while (0)

#define FCL_TEST_EXPECT_FALSE(expr, status) \
    do { \
        if (expr) { \
            FCL_LOG_ERROR("[TEST] EXPECT_FALSE failed: %s", #expr); \
            return (status); \
        } \
    } while (0)

#define FCL_TEST_EXPECT_NOT_NULL(ptr, status) \
    do { \
        if ((ptr) == nullptr) { \
            FCL_LOG_ERROR("[TEST] expected %s to be non-null", #ptr); \
            return (status); \
        } \
    } while (0)

#define FCL_TEST_EXPECT_NULL(ptr, status) \
    do { \
        if ((ptr) != nullptr) { \
            FCL_LOG_ERROR("[TEST] expected %s to be null", #ptr); \
            return (status); \
        } \
    } while (0)

#define FCL_TEST_EXPECT_NT_SUCCESS(call) \
    do { \
        const NTSTATUS _fcl_test_status = (call); \
        if (!NT_SUCCESS(_fcl_test_status)) { \
            FCL_LOG_ERROR("[TEST] %s failed with 0x%X", #call, _fcl_test_status); \
            return _fcl_test_status; \
        } \
    } while (0)

#define FCL_TEST_EXPECT_STATUS(call, expected) \
    do { \
        const NTSTATUS _fcl_test_status = (call); \
        if (_fcl_test_status != (expected)) { \
            FCL_LOG_ERROR("[TEST] %s returned 0x%X (expected 0x%X)", #call, _fcl_test_status, (expected)); \
            return _fcl_test_status; \
        } \
    } while (0)

#define FCL_TEST_EXPECT_FLOAT_NEAR(actual, expected, tolerance, status) \
    do { \
        const float _fcl_test_actual = static_cast<float>(actual); \
        const float _fcl_test_expected = static_cast<float>(expected); \
        const float _fcl_test_delta = std::fabs(_fcl_test_actual - _fcl_test_expected); \
        if (_fcl_test_delta > static_cast<float>(tolerance)) { \
            FCL_LOG_ERROR( \
                "[TEST] expected %s (%f) ~= %s (%f) with tolerance %f (delta=%f)", \
                #actual, \
                _fcl_test_actual, \
                #expected, \
                _fcl_test_expected, \
                static_cast<float>(tolerance), \
                _fcl_test_delta); \
            return (status); \
        } \
    } while (0)
