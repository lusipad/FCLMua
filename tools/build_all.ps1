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
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..')).ProviderPath

Write-Host "[1/4] Building + signing kernel driver ($Configuration|$Platform)..." -ForegroundColor Cyan
& (Join-Path $scriptDir 'build_and_sign_driver.ps1') -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "build_and_sign_driver.ps1 failed with exit code $LASTEXITCODE."
}

Write-Host "[2/4] Building CLI demo (tools\fcl_demo.exe)..." -ForegroundColor Cyan
& (Join-Path $scriptDir 'build_demo.cmd')
if ($LASTEXITCODE -ne 0) {
    throw "build_demo.cmd failed with exit code $LASTEXITCODE."
}

Write-Host "[3/4] Building GUI demo (tools\gui_demo)..." -ForegroundColor Cyan
Push-Location (Join-Path $scriptDir 'gui_demo')
try {
    & '.\build_gui_demo.cmd'
    if ($LASTEXITCODE -ne 0) {
        throw "build_gui_demo.cmd failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host ''
Write-Host "All builds completed successfully." -ForegroundColor Green
Write-Host "  Driver (dist):  dist\driver\$Platform\$Configuration\FclMusaDriver.sys"
Write-Host "  CLI demo:       tools\build\fcl_demo.exe"
Write-Host "  GUI demo:       tools\gui_demo\build\Release\fcl_gui_demo.exe"
