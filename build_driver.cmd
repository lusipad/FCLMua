@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
REM FCL+Musa Driver Build Script
REM �˽ű�����ȷ�� Visual Studio �����й�������

echo ========================================
echo FCL+Musa Driver Build Script
echo ========================================
echo.

set "FCL_EXPECTED_COMMIT=5f7776e2101b8ec95d5054d732684d00dac45e3d"
set "FCL_SOURCE_DIR=%SCRIPT_DIR%fcl-source"

if not exist "%FCL_SOURCE_DIR%" (
    echo ERROR: δ�ҵ� %FCL_SOURCE_DIR% ��Ŀ¼���޷���֤ FCL Դ������
    exit /b 1
)

for /f "usebackq delims=" %%i in (`git -C "%FCL_SOURCE_DIR%" rev-parse HEAD 2^>nul`) do (
    set "FCL_ACTUAL_COMMIT=%%i"
)

if not defined FCL_ACTUAL_COMMIT (
    echo ERROR: �޷���ȡ fcl-source �ύ�����鿴 git �Ƿ�ͨ������Ŀ¼�Ƿ��� git �ֿ⡣
    exit /b 1
)

if /i not "%FCL_ACTUAL_COMMIT%"=="%FCL_EXPECTED_COMMIT%" (
    echo ERROR: fcl-source ��ǰ�ύ %FCL_ACTUAL_COMMIT% ��δƥ��ָ��� commit %FCL_EXPECTED_COMMIT%��
    echo        ��ͬ��ָ����ύ�󽫽���ܹ���
    exit /b 1
)

REM ���� Visual Studio 2022 ����
echo [1/5] ��ʼ�� Visual Studio 2022 ����...
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64

if errorlevel 1 (
    echo ����: �޷���ʼ�� Visual Studio ����
    exit /b 1
)

echo.
echo [2/5] ���� Windows Driver Kit ����...
set "WDK_VERSION=10.0.26100.0"
set "WINDOWS_KITS_ROOT=%ProgramFiles(x86)%\Windows Kits\10"
set "WDK_INCLUDE_DIR=%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\km"

if not exist "%WDK_INCLUDE_DIR%\ntddk.h" (
    echo ����: δ�ҵ� "%WDK_INCLUDE_DIR%\ntddk.h"����ȷ���Ѱ�װƥ��汾�� Windows Driver Kit��
    exit /b 1
)

set "WDK_LIB_DIR=%WINDOWS_KITS_ROOT%\Lib\%WDK_VERSION%"
set "INCLUDE=%WDK_INCLUDE_DIR%;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\shared;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\ucrt;%WINDOWS_KITS_ROOT%\Include\%WDK_VERSION%\um;%INCLUDE%"
set "LIB=%WDK_LIB_DIR%\km\x64;%WDK_LIB_DIR%\um\x64;%LIB%"
set "PATH=%WINDOWS_KITS_ROOT%\bin\%WDK_VERSION%\x64;%PATH%"

echo   Windows Kits Ŀ¼ : %WINDOWS_KITS_ROOT%
echo   WDK �汾         : %WDK_VERSION%
echo   KM Include       : %WDK_INCLUDE_DIR%

echo.
echo [3/5] ����֮ǰ�Ĺ���...
cd /d "%SCRIPT_DIR%kernel\FclMusaDriver"
msbuild FclMusaDriver.sln /t:Clean /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo

echo.
echo [4/5] �������� (Debug x64)...
msbuild FclMusaDriver.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /v:normal /nologo

if errorlevel 1 (
    echo.
    echo ========================================
    echo ����ʧ�ܣ����������Ϣ��
    echo ========================================
    exit /b 1
)

set "DRIVER_OUTPUT=%SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\FclMusaDriver.sys"
set "SIGN_SCRIPT=%SCRIPT_DIR%tools\sign_driver.ps1"

if not exist "%DRIVER_OUTPUT%" (
    echo ����: δ�ҵ� %DRIVER_OUTPUT%
    exit /b 1
)

echo.
echo [5/5] ǩ�� FclMusaDriver.sys ...

if not exist "%SIGN_SCRIPT%" (
    echo ����: δ�ҵ�ǩ���ű� %SIGN_SCRIPT%
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%SIGN_SCRIPT%" -DriverPath "%DRIVER_OUTPUT%"

if errorlevel 1 (
    echo ����: ǩ��ʧ�ܣ���鿴�ű��������ϸ��Ϣ��
    exit /b 1
)

echo.
echo ========================================
echo �����ɹ���
echo ========================================
echo.
echo ���Ŀ¼: %SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\
echo.

REM �г����ɵ��ļ�
echo ���ɵ��ļ�:
dir /b "%SCRIPT_DIR%kernel\FclMusaDriver\out\x64\Debug\FclMusaDriver.*" 2>nul

echo.
if /i "%FCL_MUSA_NO_PAUSE%"=="1" goto :EOF
pause
