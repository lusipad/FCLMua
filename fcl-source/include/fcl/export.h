#pragma once

#if defined(_MSC_VER)
#  define FCL_HELPER_DLL_IMPORT __declspec(dllimport)
#  define FCL_HELPER_DLL_EXPORT __declspec(dllexport)
#  define FCL_HELPER_DLL_LOCAL
#else
#  if __GNUC__ >= 4
#    define FCL_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#    define FCL_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#    define FCL_HELPER_DLL_LOCAL  __attribute__((visibility("hidden")))
#  else
#    define FCL_HELPER_DLL_IMPORT
#    define FCL_HELPER_DLL_EXPORT
#    define FCL_HELPER_DLL_LOCAL
#  endif
#endif

#ifdef FCL_STATIC_DEFINE
#  define FCL_EXPORT
#  define FCL_NO_EXPORT
#else
#  ifdef FCL_EXPORTS
#    define FCL_EXPORT FCL_HELPER_DLL_EXPORT
#  else
#    define FCL_EXPORT FCL_HELPER_DLL_IMPORT
#  endif
#  define FCL_NO_EXPORT FCL_HELPER_DLL_LOCAL
#endif

#if !defined(FCL_DEPRECATED)
#  if defined(_MSC_VER)
#    define FCL_DEPRECATED __declspec(deprecated)
#  else
#    define FCL_DEPRECATED __attribute__((deprecated))
#  endif
#endif

#if !defined(FCL_DEPRECATED_EXPORT)
#  define FCL_DEPRECATED_EXPORT FCL_EXPORT FCL_DEPRECATED
#endif

#if !defined(FCL_DEPRECATED_NO_EXPORT)
#  define FCL_DEPRECATED_NO_EXPORT FCL_NO_EXPORT FCL_DEPRECATED
#endif

#if defined(FCL_NO_DEPRECATED)
#  undef FCL_DEPRECATED
#  define FCL_DEPRECATED
#  undef FCL_DEPRECATED_EXPORT
#  define FCL_DEPRECATED_EXPORT FCL_EXPORT
#  undef FCL_DEPRECATED_NO_EXPORT
#  define FCL_DEPRECATED_NO_EXPORT FCL_NO_EXPORT
#endif
