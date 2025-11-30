@echo off
setlocal

echo Attempting to find MSBuild...

set "MSBUILD_PATH="

REM Try VS2022 Community
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
)

REM Try VS2022 Enterprise
if not defined MSBUILD_PATH if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
)

REM Try VS2022 Professional
if not defined MSBUILD_PATH if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
)

REM Try VS2022 BuildTools
if not defined MSBUILD_PATH if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
)

REM Try VS2019 Community
if not defined MSBUILD_PATH if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
)

if defined MSBUILD_PATH (
    echo Found MSBuild at: "%MSBUILD_PATH%"
    "%MSBUILD_PATH%" FclGuiDemo.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
) else (
    echo Could not find MSBuild in standard locations.
    exit /b 1
)
