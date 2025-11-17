@echo off
setlocal EnableExtensions

rem FclMusaDriver one-click build + sign + package (Debug|x64)

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."
set "KERNEL_DIR=%REPO_ROOT%\kernel\FclMusaDriver"
set "OUTPUT_DIR=%KERNEL_DIR%\out\x64\Debug"
set "DRIVER_SYS=%OUTPUT_DIR%\FclMusaDriver.sys"
set "DRIVER_PDB=%OUTPUT_DIR%\FclMusaDriver.pdb"
set "DIST_DIR=%REPO_ROOT%\dist\driver\x64\Debug"

echo [1/3] Building driver solution Debug|x64 ...
call "%SCRIPT_DIR%manual_build.cmd"
if errorlevel 1 (
    echo Build failed. See kernel\FclMusaDriver\build_manual_build.log for details.
    goto :eof
)

if not exist "%DRIVER_SYS%" (
    echo Driver SYS not found: "%DRIVER_SYS%"
    goto :eof
)

echo [2/3] Signing driver: %DRIVER_SYS% ...
powershell.exe -NoLogo -ExecutionPolicy Bypass -Command ^
  "& '%SCRIPT_DIR%sign_driver.ps1' -DriverPath '%DRIVER_SYS%'"
if errorlevel 1 (
    echo Sign step failed.
    goto :eof
)

echo [3/3] Packaging artifacts to %DIST_DIR% ...
if not exist "%DIST_DIR%" (
    mkdir "%DIST_DIR%" 1>nul 2>nul
)

copy /Y "%DRIVER_SYS%" "%DIST_DIR%\" >nul
if exist "%DRIVER_PDB%" copy /Y "%DRIVER_PDB%" "%DIST_DIR%\" >nul
if exist "%OUTPUT_DIR%\FclMusaTestCert.cer" copy /Y "%OUTPUT_DIR%\FclMusaTestCert.cer" "%DIST_DIR%\" >nul
if exist "%OUTPUT_DIR%\FclMusaTestCert.pfx" copy /Y "%OUTPUT_DIR%\FclMusaTestCert.pfx" "%DIST_DIR%\" >nul

echo Done. Signed driver and symbols are under:
echo   %DIST_DIR%

endlocal
exit /b 0
