#include "fcl/narrowphase/detail/failed_at_this_configuration.h"

#if FCL_ENABLE_STD_LOGGING
#include <sstream>
#endif

namespace fcl {
namespace detail {

void ThrowFailedAtThisConfiguration(const std::string& message,
                                    const char* func,
                                    const char* file, int line) {
#if FCL_ENABLE_STD_LOGGING
  std::stringstream ss;
  ss << file << ":(" << line << "): " << func << "(): " << message;
  throw FailedAtThisConfiguration(ss.str());
#else
  (void)func;
  (void)file;
  (void)line;
  throw FailedAtThisConfiguration(message);
#endif
}

}  // namespace detail
}  // namespace fcl
