cmake_minimum_required(VERSION 3.24)
project(FclMusa LANGUAGES C CXX)

option(FCLMUSA_BUILD_DRIVER "Build kernel driver (.sys). Off by default for CPM users." OFF)
option(FCLMUSA_BUILD_USERLIB "Build user-mode static library." ON)

set(FCLMUSA_WDK_ROOT "$ENV{WDKContentRoot}" CACHE PATH "WDK root (contains Include/<version>/km)")
if(NOT FCLMUSA_WDK_VERSION AND DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
  set(FCLMUSA_WDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" CACHE STRING "WDK include version (e.g. 10.0.22621.0)" FORCE)
else()
  set(FCLMUSA_WDK_VERSION "${FCLMUSA_WDK_VERSION}" CACHE STRING "WDK include version (e.g. 10.0.22621.0)")
endif()

# Best-effort defaults when user未显式提供 WDK 配置
if((NOT DEFINED FCLMUSA_WDK_ROOT) OR (FCLMUSA_WDK_ROOT STREQUAL ""))
  set(_fclmusa_wdk_candidates
    "$ENV{WDKContentRoot}"
    "C:/Program Files (x86)/Windows Kits/10"
  )
  foreach(_cand IN LISTS _fclmusa_wdk_candidates)
    if(_cand AND EXISTS "${_cand}/Include")
      set(FCLMUSA_WDK_ROOT "${_cand}" CACHE PATH "WDK root (contains Include/<version>/km)" FORCE)
      break()
    endif()
  endforeach()
endif()

if((NOT DEFINED FCLMUSA_WDK_VERSION OR FCLMUSA_WDK_VERSION STREQUAL "") AND FCLMUSA_WDK_ROOT)
  file(GLOB _fclmusa_wdk_include_dirs LIST_DIRECTORIES true "${FCLMUSA_WDK_ROOT}/Include/*/km")
  set(_fclmusa_wdk_versions "")
  foreach(_dir IN LISTS _fclmusa_wdk_include_dirs)
    get_filename_component(_parent "${_dir}" DIRECTORY)
    get_filename_component(_ver "${_parent}" NAME)
    list(APPEND _fclmusa_wdk_versions "${_ver}")
  endforeach()
  list(REMOVE_DUPLICATES _fclmusa_wdk_versions)
  list(SORT _fclmusa_wdk_versions COMPARE NATURAL)
  list(REVERSE _fclmusa_wdk_versions)
  list(LENGTH _fclmusa_wdk_versions _fclmusa_wdk_len)
  if(_fclmusa_wdk_len GREATER 0)
    list(GET _fclmusa_wdk_versions 0 _fclmusa_wdk_best)
    if(_fclmusa_wdk_best)
      set(FCLMUSA_WDK_VERSION "${_fclmusa_wdk_best}" CACHE STRING "WDK include version (e.g. 10.0.22621.0)" FORCE)
    endif()
  endif()
endif()

if(NOT DEFINED FCLMUSA_BUILD_KERNEL_LIB)
  set(_fclmusa_kernel_default OFF)
  if(FCLMUSA_WDK_ROOT)
    set(_fclmusa_kernel_default ON)
  endif()
endif()
option(FCLMUSA_BUILD_KERNEL_LIB "Build kernel-mode static library (requires WDK)." ${_fclmusa_kernel_default})

if(FCLMUSA_BUILD_DRIVER AND NOT FCLMUSA_BUILD_KERNEL_LIB)
  message(FATAL_ERROR "FCLMUSA_BUILD_DRIVER=ON requires FCLMUSA_BUILD_KERNEL_LIB=ON (kernel static library).")
endif()

if(FCLMUSA_BUILD_KERNEL_LIB AND (NOT FCLMUSA_WDK_ROOT OR NOT FCLMUSA_WDK_VERSION))
  message(FATAL_ERROR "FCLMUSA_BUILD_KERNEL_LIB=ON requires FCLMUSA_WDK_ROOT and FCLMUSA_WDK_VERSION; set them or disable the kernel library for an R3-only build.")
endif()

set(FCLMUSA_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(FCLMUSA_INCLUDE_DIR ${FCLMUSA_ROOT}/kernel/core/include)
set(FCLMUSA_DRIVER_INCLUDE_DIR ${FCLMUSA_ROOT}/kernel/driver/include)
set(FCLMUSA_EIGEN_DIR ${FCLMUSA_ROOT}/external/Eigen)
set(FCLMUSA_FCL_DIR ${FCLMUSA_ROOT}/external/fcl-source)
set(FCLMUSA_FCL_COMPAT_DIR ${FCLMUSA_ROOT}/cmake/fcl_compat)
set(FCLMUSA_FCL_INCLUDE_DIRS
  ${FCLMUSA_FCL_DIR}/include
)
if(NOT EXISTS "${FCLMUSA_FCL_DIR}/include/fcl/export.h" AND EXISTS "${FCLMUSA_FCL_COMPAT_DIR}/fcl/export.h")
  message(STATUS "FCL export header missing from source tree; using fallback stub.")
  list(APPEND FCLMUSA_FCL_INCLUDE_DIRS ${FCLMUSA_FCL_COMPAT_DIR})
endif()
set(FCLMUSA_LIBCCD_DIR ${FCLMUSA_ROOT}/external/libccd)

set(FCLMUSA_COMMON_SOURCES
  ${FCLMUSA_ROOT}/kernel/core/src/broadphase/broadphase.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/collision/collision.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/collision/continuous_collision.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/distance/distance.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/geometry/geometry_manager.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/geometry/bvh_model.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/geometry/obbrss.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/memory/pool_allocator.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/upstream/geometry_bridge.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/upstream/upstream_bridge.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/narrowphase/libccd_memory.cpp
)

set(FCLMUSA_KERNEL_ONLY_SOURCES
  ${FCLMUSA_ROOT}/kernel/core/src/driver_state.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/runtime/c_runtime_stubs.cpp
  ${FCLMUSA_ROOT}/kernel/core/src/runtime/nonpaged_new.cpp
)

set(FCLMUSA_LIBCCD_SOURCES
  ${FCLMUSA_LIBCCD_DIR}/src/ccd.c
  ${FCLMUSA_LIBCCD_DIR}/src/mpr.c
  ${FCLMUSA_LIBCCD_DIR}/src/polytope.c
  ${FCLMUSA_LIBCCD_DIR}/src/support.c
  ${FCLMUSA_LIBCCD_DIR}/src/vec3.c
)

file(GLOB_RECURSE FCLMUSA_FCL_SOURCES CONFIGURE_DEPENDS
  "${FCLMUSA_FCL_DIR}/src/*.cpp"
)

function(fclmusa_common_target_setup target)
  target_include_directories(${target}
    PUBLIC
      ${FCLMUSA_INCLUDE_DIR}
    PRIVATE
      ${FCLMUSA_DRIVER_INCLUDE_DIR}
      ${FCLMUSA_EIGEN_DIR}
      ${FCLMUSA_LIBCCD_DIR}/src
      ${FCLMUSA_LIBCCD_DIR}/src/ccd
      ${FCLMUSA_FCL_INCLUDE_DIRS}
  )
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_compile_definitions(${target}
    PRIVATE
      _HAS_EXCEPTIONS=1
      CCD_STATIC_DEFINE
      CCD_DISABLE_DEBUG_IO
      FCL_STATIC_DEFINE
      FCL_ENABLE_STD_LOGGING=0
  )
  if(MSVC)
    target_compile_options(${target} PRIVATE /bigobj /utf-8 /W4 /FS)
  endif()
endfunction()

if(FCLMUSA_BUILD_KERNEL_LIB)
  add_library(FclMusaCore STATIC
    ${FCLMUSA_COMMON_SOURCES}
    ${FCLMUSA_KERNEL_ONLY_SOURCES}
    ${FCLMUSA_LIBCCD_SOURCES}
    ${FCLMUSA_FCL_SOURCES}
  )
  add_library(FclMusa::Core ALIAS FclMusaCore)

  fclmusa_common_target_setup(FclMusaCore)

  target_compile_definitions(FclMusaCore
    PUBLIC
      FCL_MUSA_KERNEL_MODE=1
    PRIVATE
      FCL_MUSA_ENABLE_LOGGING=1
      FCL_MUSA_ENABLE_DEMO=1
      FCL_MUSA_DPC_NO_DEBUG_CRT=1
      WINVER=0x0A00
      _WIN32_WINNT=0x0A00
  )
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(FclMusaCore PUBLIC _AMD64_)
  endif()

  if(FCLMUSA_WDK_ROOT AND FCLMUSA_WDK_VERSION)
    target_include_directories(FclMusaCore PRIVATE
      "${FCLMUSA_WDK_ROOT}/Include/${FCLMUSA_WDK_VERSION}/km"
    )
  endif()
else()
  message(STATUS "FclMusa kernel-mode static library is disabled (R3-only build).")
endif()

if(FCLMUSA_BUILD_USERLIB)
  add_library(FclMusaCoreUser STATIC
    ${FCLMUSA_COMMON_SOURCES}
    ${FCLMUSA_LIBCCD_SOURCES}
    ${FCLMUSA_FCL_SOURCES}
  )
  add_library(FclMusa::CoreUser ALIAS FclMusaCoreUser)

  fclmusa_common_target_setup(FclMusaCoreUser)
  target_compile_definitions(FclMusaCoreUser
    PUBLIC
      FCL_MUSA_KERNEL_MODE=0
    PRIVATE
      FCL_MUSA_ENABLE_LOGGING=1
      FCL_MUSA_ENABLE_DEMO=1
  )
  target_link_libraries(FclMusaCoreUser PRIVATE ntdll.lib)
endif()

if(FCLMUSA_BUILD_DRIVER)
  message(STATUS "FCLMUSA_BUILD_DRIVER is ON, but driver target is not wired yet. You can extend this section to build .sys in your environment.")
endif()

enable_testing()

if(FCLMUSA_BUILD_USERLIB)
  add_executable(FclMusaR3Smoke src/tests/r3_smoke.cpp)
  target_link_libraries(FclMusaR3Smoke PRIVATE FclMusa::CoreUser)
  add_test(NAME fclmusa_r3_smoke COMMAND FclMusaR3Smoke)

  add_executable(FclMusaUserDemo samples/r3_user_demo/main.cpp)
  target_link_libraries(FclMusaUserDemo PRIVATE FclMusa::CoreUser)
  target_compile_features(FclMusaUserDemo PRIVATE cxx_std_17)
else()
  message(STATUS "User-mode library disabled; skipping R3 smoke test target.")
endif()
