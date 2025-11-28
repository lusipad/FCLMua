@echo off
setlocal EnableExtensions

rem Optional first argument: configuration (Debug/Release). Default is Debug.
set "BUILD_CONFIGURATION=%~1"
if "%BUILD_CONFIGURATION%"=="" set "BUILD_CONFIGURATION=Debug"

call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b 1

set "WDK_VERSION=10.0.22621.0"
set "WINDOWS_KITS_ROOT=%ProgramFiles(x86)%\Windows Kits\10"
set "WDK_INCLUDE=%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%"
set "WDK_LIB=%WINDOWS_KITS_ROOT%\Lib\%WDK_VERSION%"

if not exist "%WDK_INCLUDE%\km\ntddk.h" exit /b 1

set "INCLUDE=%WDK_INCLUDE%\km;%WDK_INCLUDE%\shared;%WDK_INCLUDE%\ucrt;%WDK_INCLUDE%\um;%INCLUDE%"
set "LIB=%WDK_LIB%\km\x64;%WDK_LIB%\um\x64;%LIB%"
set "PATH=%WINDOWS_KITS_ROOT%\bin\%WDK_VERSION%\x64;%PATH%"

cd /d "%~dp0..\src\kernel\FclMusaDriver"

set "MSBUILD_EXTRA_ARGS="
if not "%MUSA_RUNTIME_LIBRARY_CONFIGURATION%"=="" (
    set "MSBUILD_EXTRA_ARGS=/p:MUSA_RUNTIME_LIBRARY_CONFIGURATION=%MUSA_RUNTIME_LIBRARY_CONFIGURATION%"
)

msbuild FclMusaDriver.sln /t:Clean /p:Configuration=%BUILD_CONFIGURATION% /p:Platform=x64 %MSBUILD_EXTRA_ARGS% /v:minimal /nologo
if errorlevel 1 exit /b 1

msbuild FclMusaDriver.sln /t:Build /p:Configuration=%BUILD_CONFIGURATION% /p:Platform=x64 %MSBUILD_EXTRA_ARGS% /m /v:normal /nologo
exit /b %ERRORLEVEL%

