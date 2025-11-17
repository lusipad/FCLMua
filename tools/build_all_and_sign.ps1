[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64')]
    [string]$Platform = 'x64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "[1/3] Building + signing kernel driver ($Configuration|$Platform)..." -ForegroundColor Cyan
& (Join-Path $scriptDir 'build_and_sign_driver.ps1') -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "build_and_sign_driver.ps1 failed with exit code $LASTEXITCODE."
}

Write-Host "[2/3] Building demos (CLI + GUI)..." -ForegroundColor Cyan

& (Join-Path $scriptDir 'build_demo.cmd')
if ($LASTEXITCODE -ne 0) {
    throw "build_demo.cmd failed with exit code $LASTEXITCODE."
}

$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path -Path $msbuild -PathType Leaf)) {
    throw "MSBuild not found at expected path: $msbuild. Adjust tools/build_all_and_sign.ps1 for your VS installation."
}
& $msbuild (Join-Path $scriptDir 'gui_demo\FclGuiDemo.vcxproj') /p:Configuration=Release /p:Platform=$Platform /m /nologo
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild for FclGuiDemo.vcxproj failed with exit code $LASTEXITCODE."
}

Write-Host "[3/3] Packaging bundle (driver + demos)..." -ForegroundColor Cyan
& (Join-Path $scriptDir 'package_bundle.ps1') -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "package_bundle.ps1 failed with exit code $LASTEXITCODE."
}

Write-Host ''
Write-Host "All builds and signing completed successfully." -ForegroundColor Green
Write-Host "  Driver (signed): dist\driver\$Platform\$Configuration\FclMusaDriver.sys"
Write-Host "  Bundle:         dist\bundle\$Platform\$Configuration\"

