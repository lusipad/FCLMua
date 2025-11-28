@echo off
REM FCL GUI Demo Build Script
REM Compiles the Windows GUI collision detection demo using MSBuild

setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================
echo FCL Collision Demo - Build Script
echo ========================================
echo.

REM Find Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Error: Visual Studio not found!
    echo Please install Visual Studio 2019 or later with C++ tools.
    exit /b 1
)

REM Get VS installation path
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSINSTALLDIR=%%i"
)

if not defined VSINSTALLDIR (
    echo Error: Visual Studio C++ tools not found!
    echo Please install Visual Studio with C++ Desktop Development workload.
    exit /b 1
)

echo Found Visual Studio at: %VSINSTALLDIR%
echo.

REM Find MSBuild
set "MSBUILD=%VSINSTALLDIR%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
    echo Error: MSBuild not found!
    echo Expected at: "%MSBUILD%"
    exit /b 1
)

echo Building with MSBuild...
echo.

REM Build with MSBuild
"%MSBUILD%" FclGuiDemo.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo

if errorlevel 1 (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
    exit /b 1
)

echo.
echo ========================================
echo Build SUCCESSFUL!
echo ========================================
echo.
echo Executable: build\Release\fcl_gui_demo.exe
echo.
echo To run the demo:
echo   1. Make sure the FCL driver is loaded: sc start FclMusa
echo   2. Run: build\Release\fcl_gui_demo.exe
echo.
echo Controls:
echo   - Right Mouse: Rotate camera
echo   - Middle Mouse: Pan camera
echo   - Mouse Wheel: Zoom camera
echo   - Left Mouse: Drag selected object
echo   - 1-9 Keys: Select objects
echo   - W/S Keys: Move selected object up/down
echo   - Q/E Keys: Rotate selected object
echo   - ESC: Deselect
echo.

endlocal
