@echo off
setlocal

set VSCMD_ARG_no_logo=1
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64

if not exist "%~dp0build" mkdir "%~dp0build"
pushd "%~dp0build"

echo Compiling GUI Demo...
cl /utf-8 /EHsc /std:c++17 /nologo /W4 ..\*.cpp /link /out:fcl_demo.exe

popd
if errorlevel 1 (
    echo.
    echo Compilation FAILED
    exit /b 1
)

echo.
echo Compilation succeeded: %~dp0build\fcl_demo.exe
exit /b 0
