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

Write-Host "[1/3] Building kernel driver ($Configuration|$Platform, unsigned)..." -ForegroundColor Cyan
& (Join-Path $scriptDir 'manual_build.cmd') $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "manual_build.cmd failed with exit code $LASTEXITCODE. See kernel\FclMusaDriver\build_manual_build.log for details."
}

Write-Host "[2/3] Building CLI demo (tools\fcl_demo.exe)..." -ForegroundColor Cyan
& (Join-Path $scriptDir 'build_demo.cmd')
if ($LASTEXITCODE -ne 0) {
    throw "build_demo.cmd failed with exit code $LASTEXITCODE."
}

Write-Host "[3/3] Building GUI demo (tools\gui_demo)..." -ForegroundColor Cyan
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path -Path $msbuild -PathType Leaf)) {
    throw "MSBuild not found at expected path: $msbuild. Adjust tools/build_all.ps1 for your VS installation."
}
& $msbuild (Join-Path $scriptDir 'gui_demo\FclGuiDemo.vcxproj') /p:Configuration=Release /p:Platform=$Platform /m /nologo
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild for FclGuiDemo.vcxproj failed with exit code $LASTEXITCODE."
}

Write-Host ''
Write-Host "All builds (driver + CLI + GUI) completed successfully (driver is NOT signed)." -ForegroundColor Green

