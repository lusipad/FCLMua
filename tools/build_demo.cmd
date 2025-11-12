@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 (
    echo Failed to initialize Visual Studio environment.
    exit /b 1
)
cd /d "%~dp0"
if not exist build mkdir build
cd build
cl /EHsc /nologo /W4 ..\fcl_demo.cpp /link /out:fcl_demo.exe
endlocal
