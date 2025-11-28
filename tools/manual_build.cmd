@echo off
setlocal EnableExtensions

rem Optional first argument: configuration (Debug/Release). Default is Debug.
set "BUILD_CONFIGURATION=%~1"
if "%BUILD_CONFIGURATION%"=="" set "BUILD_CONFIGURATION=Debug"

call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b 1

set "SCRIPT_ROOT=%~dp0"
for /f "usebackq delims=" %%i in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_ROOT%build\resolve_wdk_env.ps1" -EmitBatch`) do %%i

if "%WDK_ROOT%"=="" (
    echo Failed to locate WDK installation. Please install Windows Driver Kit.
    exit /b 1
)

if defined WDK_RESOLVED_INCLUDE set "INCLUDE=%WDK_RESOLVED_INCLUDE%;%INCLUDE%"
if defined WDK_RESOLVED_LIB set "LIB=%WDK_RESOLVED_LIB%;%LIB%"
if defined WDK_RESOLVED_BIN set "PATH=%WDK_RESOLVED_BIN%;%PATH%"

cd /d "%~dp0..\kernel\FclMusaDriver"

set "MSBUILD_EXTRA_ARGS="
if not "%MUSA_RUNTIME_LIBRARY_CONFIGURATION%"=="" (
    set "MSBUILD_EXTRA_ARGS=/p:MUSA_RUNTIME_LIBRARY_CONFIGURATION=%MUSA_RUNTIME_LIBRARY_CONFIGURATION%"
)

msbuild FclMusaDriver.sln /t:Clean /p:Configuration=%BUILD_CONFIGURATION% /p:Platform=x64 /p:WindowsTargetPlatformVersion=%WDK_VERSION% %MSBUILD_EXTRA_ARGS% /v:minimal /nologo
if errorlevel 1 exit /b 1

msbuild FclMusaDriver.sln /t:Build /p:Configuration=%BUILD_CONFIGURATION% /p:Platform=x64 /p:WindowsTargetPlatformVersion=%WDK_VERSION% %MSBUILD_EXTRA_ARGS% /m /v:normal /nologo
exit /b %ERRORLEVEL%

