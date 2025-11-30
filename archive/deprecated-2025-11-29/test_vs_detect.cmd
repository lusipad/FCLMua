@echo off
setlocal

echo Testing VS detection...
echo.

rem Dynamically locate Visual Studio using inline PowerShell
set "VS_DEVCMD="
for /f "usebackq delims=" %%i in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe';if(Test-Path $p){$v=&$p -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -format value;if($v){Join-Path $v 'Common7\Tools\VsDevCmd.bat'}}"`) do set "VS_DEVCMD=%%i"

echo Result from vswhere: %VS_DEVCMD%
echo.

rem Fallback: try standard paths
if "%VS_DEVCMD%"=="" (
    echo Trying fallback paths...
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" (
        set "VS_DEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
        echo Found: Enterprise
    )
)
if "%VS_DEVCMD%"=="" (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" (
        set "VS_DEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
        echo Found: Professional
    )
)
if "%VS_DEVCMD%"=="" (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
        set "VS_DEVCMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
        echo Found: Community
    )
)

echo.
if "%VS_DEVCMD%"=="" (
    echo ERROR: Failed to locate Visual Studio installation.
    exit /b 1
) else (
    echo SUCCESS: Found Visual Studio at:
    echo %VS_DEVCMD%
    exit /b 0
)
