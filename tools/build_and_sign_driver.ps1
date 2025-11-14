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

$kernelDir = Join-Path $repoRoot 'kernel/FclMusaDriver'
$outputDir = Join-Path $kernelDir "out\$Platform\$Configuration"
$driverSys = Join-Path $outputDir 'FclMusaDriver.sys'
$driverPdb = Join-Path $outputDir 'FclMusaDriver.pdb'
$distDir = Join-Path $repoRoot "dist\driver\$Platform\$Configuration"

Write-Host "[1/3] Building driver solution ($Configuration|$Platform)..." -ForegroundColor Cyan
& (Join-Path $scriptDir 'manual_build.cmd')
if ($LASTEXITCODE -ne 0) {
    throw "manual_build.cmd failed with exit code $LASTEXITCODE. See kernel\FclMusaDriver\build_manual_build.log for details."
}

if (-not (Test-Path -Path $driverSys -PathType Leaf)) {
    throw "Driver SYS not found: $driverSys"
}

Write-Host "[2/3] Signing driver: $driverSys" -ForegroundColor Cyan
& (Join-Path $scriptDir 'sign_driver.ps1') -DriverPath $driverSys
if ($LASTEXITCODE -ne 0) {
    throw "sign_driver.ps1 failed with exit code $LASTEXITCODE."
}

Write-Host "[3/3] Packaging artifacts to $distDir" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

Copy-Item -Path $driverSys -Destination $distDir -Force
if (Test-Path -Path $driverPdb -PathType Leaf) {
    Copy-Item -Path $driverPdb -Destination $distDir -Force
}

$cerPath = Join-Path $outputDir 'FclMusaTestCert.cer'
$pfxPath = Join-Path $outputDir 'FclMusaTestCert.pfx'
if (Test-Path -Path $cerPath -PathType Leaf) {
    Copy-Item -Path $cerPath -Destination $distDir -Force
}
if (Test-Path -Path $pfxPath -PathType Leaf) {
    Copy-Item -Path $pfxPath -Destination $distDir -Force
}

Write-Host 'Done. Signed driver and symbols are under:' -ForegroundColor Green
Write-Host "  $distDir"

