@echo off
setlocal EnableExtensions

rem Build all kernel and user-mode artifacts for FCL+Musa
rem - Driver (kernel/FclMusaDriver)
rem - Demo CLI (tools/fcl_demo.exe)
rem - GUI demo (tools/gui_demo)

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."

echo [1/3] Building kernel driver (Debug|x64) ...
call "%SCRIPT_DIR%manual_build.cmd"
if errorlevel 1 (
    echo Driver build failed. See kernel\FclMusaDriver\build_manual_build.log for details.
    goto :eof
)

echo [2/3] Building CLI demo (tools\fcl_demo.exe) ...
call "%SCRIPT_DIR%build_demo.cmd"
if errorlevel 1 (
    echo CLI demo build failed.
    goto :eof
)

echo [3/3] Building GUI demo (tools\gui_demo) ...
pushd "%SCRIPT_DIR%gui_demo"
call build_gui_demo.cmd
if errorlevel 1 (
    echo GUI demo build failed.
    popd
    goto :eof
)
popd

echo.
echo All builds completed successfully.
echo  - Driver:        kernel\FclMusaDriver\out\x64\Debug\FclMusaDriver.sys
echo  - CLI demo:      tools\build\fcl_demo.exe
echo  - GUI demo:      tools\gui_demo\build\Release\fcl_gui_demo.exe

endlocal
exit /b 0

