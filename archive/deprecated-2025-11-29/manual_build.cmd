@echo off
setlocal EnableExtensions

rem Optional first argument: configuration (Debug/Release). Default is Debug.
set "BUILD_CONFIGURATION=%~1"
if "%BUILD_CONFIGURATION%"=="" set "BUILD_CONFIGURATION=Debug"

rem Dynamically locate Visual Studio using inline PowerShell
set "VS_DEVCMD="
for /f "usebackq delims=" %%i in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe';if(Test-Path $p){$v=&$p -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -format value;if($v){Join-Path $v 'Common7\Tools\VsDevCmd.bat'}}"`) do set "VS_DEVCMD=%%i"

rem Fallback: try standard paths
if "%VS_DEVCMD%"=="" (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" set "VS_DEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
)
if "%VS_DEVCMD%"=="" (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" set "VS_DEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
)
if "%VS_DEVCMD%"=="" (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" set "VS_DEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
)

if "%VS_DEVCMD%"=="" (
    echo ERROR: Failed to locate Visual Studio installation.
    echo Please install Visual Studio 2019 or later with C++ workload.
    exit /b 1
)

echo Using Visual Studio: %VS_DEVCMD%
call "%VS_DEVCMD%" -arch=amd64 -host_arch=amd64 >nul
if errorlevel 1 exit /b 1

set "SCRIPT_ROOT=%~dp0"
set "TEMP_WDK_ENV=%TEMP%\wdk_env_%RANDOM%.bat"

rem ?? PowerShell ????????????
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_ROOT%resolve_wdk_env.ps1" -EmitBatch > "%TEMP_WDK_ENV%" 2>nul
if errorlevel 1 (
    echo Failed to resolve WDK environment. Please install Windows Driver Kit.
    if exist "%TEMP_WDK_ENV%" del "%TEMP_WDK_ENV%"
    exit /b 1
)

rem ???????????????
call "%TEMP_WDK_ENV%"
del "%TEMP_WDK_ENV%"

rem Check if WDK was resolved (either WDK_ROOT or WDK_VERSION should be set)
if "%WDK_VERSION%"=="" (
    echo Failed to locate WDK installation. Please install Windows Driver Kit.
    exit /b 1
)

echo Using WDK version: %WDK_VERSION%

if defined WDK_RESOLVED_INCLUDE set "INCLUDE=%WDK_RESOLVED_INCLUDE%;%INCLUDE%"
if defined WDK_RESOLVED_LIB set "LIB=%WDK_RESOLVED_LIB%;%LIB%"
if defined WDK_RESOLVED_BIN set "PATH=%WDK_RESOLVED_BIN%;%PATH%"

cd /d "%~dp0..\r0\driver\msbuild"

set "MSBUILD_EXTRA_ARGS="
if not "%MUSA_RUNTIME_LIBRARY_CONFIGURATION%"=="" (
    set "MSBUILD_EXTRA_ARGS=/p:MUSA_RUNTIME_LIBRARY_CONFIGURATION=%MUSA_RUNTIME_LIBRARY_CONFIGURATION%"
)

msbuild FclMusaDriver.sln /t:Clean /p:Configuration=%BUILD_CONFIGURATION% /p:Platform=x64 /p:WindowsTargetPlatformVersion=%WDK_VERSION% %MSBUILD_EXTRA_ARGS% /v:minimal /nologo
if errorlevel 1 exit /b 1

msbuild FclMusaDriver.sln /t:Build /p:Configuration=%BUILD_CONFIGURATION% /p:Platform=x64 /p:WindowsTargetPlatformVersion=%WDK_VERSION% %MSBUILD_EXTRA_ARGS% /m /v:normal /nologo
exit /b %ERRORLEVEL%

