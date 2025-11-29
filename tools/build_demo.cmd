@echo off
setlocal

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
call "%VS_DEVCMD%" -arch=amd64 -host_arch=amd64
if errorlevel 1 (
    echo Failed to initialize Visual Studio environment.
    exit /b 1
)
cd /d "%~dp0"
if not exist build mkdir build
cd build
cl /utf-8 /EHsc /std:c++17 /nologo /W4 ..\fcl_demo.cpp /link /out:fcl_demo.exe
if errorlevel 1 (
    echo Failed to build fcl_demo.exe
    exit /b 1
)

echo Copying demo assets...
robocopy "..\assets" "assets" /E /NFL /NDL /NJH /NJS /NC /NS >nul
if errorlevel 8 (
    echo Failed to copy assets directory.
    exit /b 1
)

echo Copying demo scenes...
robocopy "..\scenes" "scenes" /E /NFL /NDL /NJH /NJS /NC /NS >nul
if errorlevel 8 (
    echo Failed to copy scenes directory.
    exit /b 1
)
endlocal
