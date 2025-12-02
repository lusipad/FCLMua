# FclMusaCPMTemplate.cmake
#
# 将本文件拷贝到你的工程（例如 <repo>/cmake/FclMusaCPMTemplate.cmake），
# 然后在顶层 CMakeLists.txt 中 `include()` 并根据需要修改下列开关。
# 该模板负责：
#   1. 通过 CPM 拉取 FCL+Musa；
#   2. 自动套用 upstream FCL 补丁；
#   3. 在启用 R0 构建时恢复 Musa.Runtime（标准库）依赖；
#   4. 暴露 `FclMusa::Core` / `FclMusa::CoreUser` 供上层 target 链接。
#
# 先决条件：
#   - CMake ≥ 3.24
#   - Windows + PowerShell 7 (`pwsh` 在 PATH 中)
#   - WDK（若启用 FCLMUSA_CPM_ENABLE_R0）

set(FCLMUSA_CPM_VERSION "v0.1.0" CACHE STRING "FCL+Musa tag/commit used by CPM")
set(FCLMUSA_CPM_ENABLE_R3 ON CACHE BOOL "Build user-mode static library FclMusa::CoreUser via CPM")
set(FCLMUSA_CPM_ENABLE_R0 OFF CACHE BOOL "Build kernel-mode static library FclMusa::Core via CPM")
set(FCLMUSA_CPM_ENABLE_DRIVER OFF CACHE BOOL "Enable driver (.sys) wiring (placeholder, still MSBuild-based)")

if(FCLMUSA_CPM_ENABLE_R0)
    set(FCLMUSA_CPM_WDK_ROOT "C:/Program Files (x86)/Windows Kits/10" CACHE PATH "WDK root directory")
    set(FCLMUSA_CPM_WDK_VERSION "10.0.22621.0" CACHE STRING "WDK include version (e.g. 10.0.22621.0)")
endif()

if(NOT COMMAND CPMAddPackage)
    include(cmake/CPM.cmake)
endif()

CPMAddPackage(
    NAME FclMusa
    GITHUB_REPOSITORY lusipad/FCLMua
    GIT_TAG ${FCLMUSA_CPM_VERSION}
    OPTIONS
        "FCLMUSA_BUILD_USERLIB ${FCLMUSA_CPM_ENABLE_R3}"
        "FCLMUSA_BUILD_KERNEL_LIB ${FCLMUSA_CPM_ENABLE_R0}"
        "FCLMUSA_BUILD_DRIVER ${FCLMUSA_CPM_ENABLE_DRIVER}"
        "FCLMUSA_WDK_ROOT ${FCLMUSA_CPM_WDK_ROOT}"
        "FCLMUSA_WDK_VERSION ${FCLMUSA_CPM_WDK_VERSION}"
)

if(FclMusa_ADDED)
    set(_fclmusa_scripts "${FclMusa_SOURCE_DIR}/tools/scripts")

    execute_process(
        COMMAND pwsh -NoLogo -NoProfile -File apply_fcl_patch.ps1
        WORKING_DIRECTORY "${_fclmusa_scripts}"
        RESULT_VARIABLE _fclmusa_patch_rv
    )
    if(NOT _fclmusa_patch_rv EQUAL 0)
        message(FATAL_ERROR "FCL+Musa CPM template: apply_fcl_patch.ps1 failed with exit code ${_fclmusa_patch_rv}")
    endif()

    if(FCLMUSA_CPM_ENABLE_R0)
        execute_process(
            COMMAND pwsh -NoLogo -NoProfile -File setup_dependencies.ps1
            WORKING_DIRECTORY "${_fclmusa_scripts}"
            RESULT_VARIABLE _fclmusa_deps_rv
        )
        if(NOT _fclmusa_deps_rv EQUAL 0)
            message(FATAL_ERROR "FCL+Musa CPM template: setup_dependencies.ps1 failed with exit code ${_fclmusa_deps_rv}")
        endif()
    endif()
endif()

