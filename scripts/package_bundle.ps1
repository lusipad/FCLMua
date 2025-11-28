[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64')]
    [string]$Platform = 'x64'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..')).ProviderPath

$driverDistDir = Join-Path $repoRoot "dist\driver\$Platform\$Configuration"
$bundleDir = Join-Path $repoRoot "dist\bundle\$Platform\$Configuration"

Write-Host "Packaging artifacts to $bundleDir..." -ForegroundColor Cyan

if (-not (Test-Path -Path $driverDistDir -PathType Container)) {
    throw "Driver dist directory not found: $driverDistDir. Build the driver first (tools/build_all.ps1 or build_and_sign_driver.ps1)."
}

New-Item -ItemType Directory -Force -Path $bundleDir | Out-Null

# Driver: SYS/PDB/Certs
Get-ChildItem -Path $driverDistDir -File | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $bundleDir -Force
}

# CLI demo
$cliDemo = Join-Path $scriptDir 'build\fcl_demo.exe'
if (Test-Path -Path $cliDemo -PathType Leaf) {
    Copy-Item -Path $cliDemo -Destination $bundleDir -Force
}

# GUI demo（Release x64，目前仅此配置）
$guiDemo = Join-Path $scriptDir 'gui_demo\build\Release\fcl_gui_demo.exe'
if (Test-Path -Path $guiDemo -PathType Leaf) {
    Copy-Item -Path $guiDemo -Destination $bundleDir -Force
}

Write-Host "Bundle created at: $bundleDir" -ForegroundColor Green
Write-Host "  Contains driver SYS/PDB/certs and CLI/GUI demos (if built)."

