@echo off
REM FCL GUI Demo Build Script
REM Compiles the Windows GUI collision detection demo

setlocal enabledelayedexpansion

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

REM Setup environment
call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo Error: Failed to setup Visual Studio environment
    exit /b 1
)

REM Create build directory
if not exist "build" mkdir build
pushd build

echo Compiling source files...
echo.

REM Compiler flags
set CFLAGS=/nologo /W3 /EHsc /std:c++17 /O2 /I..\src
set LIBS=d3d11.lib d3dcompiler.lib dxgi.lib user32.lib gdi32.lib kernel32.lib

REM Source files
set SOURCES=^
    ..\src\main.cpp ^
    ..\src\window.cpp ^
    ..\src\renderer.cpp ^
    ..\src\camera.cpp ^
    ..\src\scene.cpp ^
    ..\src\fcl_driver.cpp

REM Compile and link
echo cl %CFLAGS% %SOURCES% /link %LIBS% /OUT:fcl_gui_demo.exe
cl %CFLAGS% %SOURCES% /link %LIBS% /OUT:fcl_gui_demo.exe

if errorlevel 1 (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
    popd
    exit /b 1
)

echo.
echo ========================================
echo Build SUCCESSFUL!
echo ========================================
echo.
echo Executable: build\fcl_gui_demo.exe
echo.
echo To run the demo:
echo   1. Make sure the FCL driver is loaded: sc start FclMusa
echo   2. Run: build\fcl_gui_demo.exe
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

popd
endlocal
