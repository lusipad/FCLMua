#pragma once

#include <cstddef>

#include "fclmusa/memory/pool_allocator.h"

#ifndef __has_include
#define __has_include(x) 0
#endif

#if !defined(FCL_MUSA_EIGEN_ENABLED)
#define FCL_MUSA_EIGEN_ENABLED __has_include(<Eigen/Core>)
#endif

#if FCL_MUSA_EIGEN_ENABLED

#define EIGEN_CORE_NO_WARNING_MACRO_USAGE
#define EIGEN_NO_DEBUG
#define EIGEN_NO_STATIC_ASSERT
#define EIGEN_DONT_ALIGN
#define EIGEN_DONT_VECTORIZE
#define EIGEN_HAS_C99_MATH 1
#define EIGEN_MAX_ALIGN_BYTES 0
#define EIGEN_MPL2_ONLY

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/SVD>
#include <Eigen/Eigenvalues>

#ifndef EIGEN_NO_DEBUG
#define EIGEN_NO_DEBUG
#endif

#ifndef EIGEN_NO_STATIC_ASSERT
#define EIGEN_NO_STATIC_ASSERT
#endif

#ifndef EIGEN_DONT_ALIGN
#define EIGEN_DONT_ALIGN
#endif

#ifndef EIGEN_DONT_VECTORIZE
#define EIGEN_DONT_VECTORIZE
#endif

#ifndef EIGEN_HAS_C99_MATH
#define EIGEN_HAS_C99_MATH 1
#endif

namespace fclmusa::math {

inline void* EigenAllocate(size_t size) noexcept {
    return fclmusa::memory::Allocate(size);
}

inline void EigenFree(void* buffer) noexcept {
    fclmusa::memory::Free(buffer);
}

}  // namespace fclmusa::math

#define EIGEN_MALLOC(size) fclmusa::math::EigenAllocate(size)
#define EIGEN_FREE(ptr)    fclmusa::math::EigenFree(ptr)

#endif  // FCL_MUSA_EIGEN_ENABLED
