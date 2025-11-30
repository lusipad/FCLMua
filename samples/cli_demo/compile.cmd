@echo off
setlocal
set VSCMD_ARG_no_logo=1
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 (
    echo ERROR: VsDevCmd failed
    exit /b 1
)
cd /d "%~dp0"
cl /utf-8 /EHsc /std:c++17 /nologo /W4 main.cpp /link /out:fcl_demo.exe
exit /b %ERRORLEVEL%
