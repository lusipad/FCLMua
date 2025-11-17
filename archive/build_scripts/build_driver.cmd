@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
REM FCL+Musa Driver Build Script
REM 锟剿脚憋拷锟斤拷锟斤拷确锟斤拷 Visual Studio 锟斤拷锟斤拷锟叫癸拷锟斤拷锟斤拷锟斤拷

echo ========================================
echo FCL+Musa Driver Build Script
echo ========================================
echo.

set "FCL_EXPECTED_COMMIT=5f7776e2101b8ec95d5054d732684d00dac45e3d"
set "FCL_SOURCE_DIR=%SCRIPT_DIR%fcl-source"

if not exist "%FCL_SOURCE_DIR%" (
    echo ERROR: 未锟揭碉拷 %FCL_SOURCE_DIR% 锟斤拷目录锟斤拷锟睫凤拷锟斤拷证 FCL 源锟斤拷锟斤拷锟斤拷
    exit /b 1
)

for /f "usebackq delims=" %%i in (`git -C "%FCL_SOURCE_DIR%" rev-parse HEAD 2^>nul`) do (
    set "FCL_ACTUAL_COMMIT=%%i"
)

if not defined FCL_ACTUAL_COMMIT (
    echo ERROR: 锟睫凤拷锟斤拷取 fcl-source 锟结交锟斤拷锟斤拷锟介看 git 锟角凤拷通锟斤拷锟斤拷锟斤拷目录锟角凤拷锟斤拷 git 锟街库。
    exit /b 1
)

if /i not "%FCL_ACTUAL_COMMIT%"=="%FCL_EXPECTED_COMMIT%" (
    echo ERROR: fcl-source 锟斤拷前锟结交 %FCL_ACTUAL_COMMIT% 锟斤拷未匹锟斤拷指锟斤拷锟� commit %FCL_EXPECTED_COMMIT%锟斤拷
    echo        锟斤拷同锟斤拷指锟斤拷锟斤拷峤伙拷蠼锟斤拷锟杰癸拷锟斤拷
    exit /b 1
)

REM 锟斤拷锟斤拷 Visual Studio 2022 锟斤拷锟斤拷
echo [1/5] 锟斤拷始锟斤拷 Visual Studio 2022 锟斤拷锟斤拷...
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64

if errorlevel 1 (
    echo 锟斤拷锟斤拷: 锟睫凤拷锟斤拷始锟斤拷 Visual Studio 锟斤拷锟斤拷
    exit /b 1
)

echo.
echo [2/5] 锟斤拷锟斤拷 Windows Driver Kit 锟斤拷锟斤拷...
set "WDK_VERSION=10.0.26100.0"
set "WINDOWS_KITS_ROOT=%ProgramFiles(x86)%\Windows Kits\10"
set "WDK_INCLUDE_DIR=%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\km"

if not exist "%WDK_INCLUDE_DIR%\ntddk.h" (
    echo 锟斤拷锟斤拷: 未锟揭碉拷 "%WDK_INCLUDE_DIR%\ntddk.h"锟斤拷锟斤拷确锟斤拷锟窖帮拷装匹锟斤拷姹撅拷锟� Windows Driver Kit锟斤拷
    exit /b 1
)

set "WDK_LIB_DIR=%WINDOWS_KITS_ROOT%\Lib\%WDK_VERSION%"
set "INCLUDE=%WDK_INCLUDE_DIR%;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\shared;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\ucrt;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\um;%INCLUDE%"
set "LIB=%WDK_LIB_DIR%\km\x64;%WDK_LIB_DIR%\um\x64;%LIB%"
set "PATH=%WINDOWS_KITS_ROOT%\bin\%WDK_VERSION%\x64;%PATH%"

echo   Windows Kits 目录 : %WINDOWS_KITS_ROOT%
echo   WDK 锟芥本         : %WDK_VERSION%
echo   KM Include       : %WDK_INCLUDE_DIR%

echo.
echo [3/5] 锟斤拷锟斤拷之前锟侥癸拷锟斤拷...
cd /d "%SCRIPT_DIR%kernel\FclMusaDriver"
msbuild FclMusaDriver.sln /t:Clean /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo

echo.
echo [4/5] 锟斤拷锟斤拷锟斤拷锟斤拷 (Debug x64)...
msbuild FclMusaDriver.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /v:normal /nologo

if errorlevel 1 (
    echo.
    echo ========================================
    echo 锟斤拷锟斤拷失锟杰ｏ拷锟斤拷锟斤拷锟斤拷锟斤拷锟较拷锟�
    echo ========================================
    exit /b 1
)

set "DRIVER_OUTPUT=%SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\FclMusaDriver.sys"
set "SIGN_SCRIPT=%SCRIPT_DIR%tools\sign_driver.ps1"

if not exist "%DRIVER_OUTPUT%" (
    echo 锟斤拷锟斤拷: 未锟揭碉拷 %DRIVER_OUTPUT%
    exit /b 1
)

echo.
echo [5/5] 签锟斤拷 FclMusaDriver.sys ...

if not exist "%SIGN_SCRIPT%" (
    echo 锟斤拷锟斤拷: 未锟揭碉拷签锟斤拷锟脚憋拷 %SIGN_SCRIPT%
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%SIGN_SCRIPT%" -DriverPath "%DRIVER_OUTPUT%"

if errorlevel 1 (
    echo 锟斤拷锟斤拷: 签锟斤拷失锟杰ｏ拷锟斤拷榭达拷疟锟斤拷锟斤拷锟斤拷锟斤拷细锟斤拷息锟斤拷
    exit /b 1
)

echo.
echo ========================================
echo 锟斤拷锟斤拷锟缴癸拷锟斤拷
echo ========================================
echo.
echo 锟斤拷锟侥柯�: %SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\
echo.

REM 锟叫筹拷锟斤拷锟缴碉拷锟侥硷拷
echo 锟斤拷锟缴碉拷锟侥硷拷:
dir /b "%SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\FclMusaDriver.*" 2>nul

echo.
if /i "%FCL_MUSA_NO_PAUSE%"=="1" goto :EOF
pause
