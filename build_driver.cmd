@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
REM FCL+Musa Driver Build Script
REM 此脚本在正确的 Visual Studio 环境中构建驱动

echo ========================================
echo FCL+Musa Driver Build Script
echo ========================================
echo.

REM 设置 Visual Studio 2022 环境
echo [1/4] 初始化 Visual Studio 2022 环境...
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64

if errorlevel 1 (
    echo 错误: 无法初始化 Visual Studio 环境
    exit /b 1
)

echo.
echo [2/4] 配置 Windows Driver Kit 环境...
set "WDK_VERSION=10.0.26100.0"
set "WINDOWS_KITS_ROOT=%ProgramFiles(x86)%\Windows Kits\10"
set "WDK_INCLUDE_DIR=%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\km"

if not exist "%WDK_INCLUDE_DIR%\ntddk.h" (
    echo 错误: 未找到 "%WDK_INCLUDE_DIR%\ntddk.h"，请确认已安装匹配版本的 Windows Driver Kit。
    exit /b 1
)

set "WDK_LIB_DIR=%WINDOWS_KITS_ROOT%\Lib\%WDK_VERSION%"
set "INCLUDE=%WDK_INCLUDE_DIR%;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\shared;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\ucrt;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\um;%INCLUDE%"
set "LIB=%WDK_LIB_DIR%\km\x64;%WDK_LIB_DIR%\um\x64;%LIB%"
set "PATH=%WINDOWS_KITS_ROOT%\bin\%WDK_VERSION%\x64;%PATH%"

echo   Windows Kits 目录 : %WINDOWS_KITS_ROOT%
echo   WDK 版本         : %WDK_VERSION%
echo   KM Include       : %WDK_INCLUDE_DIR%

echo.
echo [3/4] 清理之前的构建...
cd /d "%SCRIPT_DIR%kernel\FclMusaDriver"
msbuild FclMusaDriver.sln /t:Clean /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo

echo.
echo [4/4] 构建驱动 (Debug x64)...
msbuild FclMusaDriver.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /v:normal /nologo

if errorlevel 1 (
    echo.
    echo ========================================
    echo 构建失败！请检查错误信息。
    echo ========================================
    exit /b 1
)

echo.
echo ========================================
echo 构建成功！
echo ========================================
echo.
echo 输出目录: %SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\
echo.

REM 列出生成的文件
echo 生成的文件:
dir /b "%SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\FclMusaDriver.*" 2>nul

echo.
if /i "%FCL_MUSA_NO_PAUSE%"=="1" goto :EOF
pause
