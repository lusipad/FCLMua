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
cl /EHsc /std:c++17 /nologo /W4 ..\fcl_demo.cpp /link /out:fcl_demo.exe
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
