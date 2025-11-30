#pragma once

// Generated stub for kernel/user builds: OctoMap is not bundled in this environment.
#define FCL_HAVE_OCTOMAP 0

#ifndef FCL_ENABLE_PROFILING
#define FCL_ENABLE_PROFILING 0
#endif

#ifndef FCL_ENABLE_STD_IOSTREAM
#if defined(_KERNEL_MODE)
#define FCL_ENABLE_STD_IOSTREAM 0
#else
#define FCL_ENABLE_STD_IOSTREAM 1
#endif
#endif

#if FCL_ENABLE_STD_IOSTREAM
#define FCL_STDOUT std::cout
#define FCL_STDERR std::cerr
#else

#include <type_traits>
#include <utility>
#include <iosfwd>

namespace fcl {
namespace detail {
class NullOStream {
 public:
  template <typename T>
  NullOStream& operator<<(const T&) {
    return *this;
  }

  template <typename CharT, typename Traits>
  NullOStream& operator<<(std::basic_ostream<CharT, Traits>& (*)(std::basic_ostream<CharT, Traits>&)) {
    return *this;
  }

  NullOStream& operator<<(std::ios_base& (*)(std::ios_base&)) {
    return *this;
  }

  template <typename CharT, typename Traits>
  NullOStream& operator<<(std::basic_ios<CharT, Traits>& (*)(std::basic_ios<CharT, Traits>&)) {
    return *this;
  }
};

inline NullOStream& GetNullOStream() {
  static NullOStream stream;
  return stream;
}
}  // namespace detail
}  // namespace fcl

#define FCL_STDOUT ::fcl::detail::GetNullOStream()
#define FCL_STDERR ::fcl::detail::GetNullOStream()
#endif
