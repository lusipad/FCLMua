@echo off
setlocal EnableExtensions

call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 goto :vs_error

set "WDK_VERSION=10.0.26100.0"
set WINDOWS_KITS_ROOT=%ProgramFiles(x86)%\Windows Kits\10
set "WDK_INCLUDE=%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%"
set "WDK_LIB=%WINDOWS_KITS_ROOT%\Lib\%WDK_VERSION%"

if not exist "%WDK_INCLUDE%\km\ntddk.h" goto :missing_wdk

set "INCLUDE=%WDK_INCLUDE%\km;%WDK_INCLUDE%\shared;%WDK_INCLUDE%\ucrt;%WDK_INCLUDE%\um;%INCLUDE%"
set "LIB=%WDK_LIB%\km\x64;%WDK_LIB%\um\x64;%LIB%"
set "PATH=%WINDOWS_KITS_ROOT%\bin\%WDK_VERSION%\x64;%PATH%"

cd /d "%~dp0..\kernel\FclMusaDriver"

msbuild FclMusaDriver.sln /t:Clean /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo
if errorlevel 1 goto :clean_failed

msbuild FclMusaDriver.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /v:normal /nologo
if errorlevel 1 goto :build_failed

echo Build succeeded.
goto :eof

:vs_error
echo Failed to initialize VsDevCmd.
exit /b 1

:missing_wdk
echo Missing WDK headers under %WDK_INCLUDE%.
exit /b 1

:clean_failed
echo Clean failed.
exit /b 1

:build_failed
echo Build failed.
exit /b 1
