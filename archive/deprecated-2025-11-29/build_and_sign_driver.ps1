[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64')]
    [string]$Platform = 'x64'
)

# Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 导入公共函数库
$scriptDir = $PSScriptRoot
if (-not $scriptDir) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
}
$commonPath = Join-Path $scriptDir 'common.psm1'
if (-not (Test-Path -Path $commonPath)) {
    throw "Common functions module not found: $commonPath"
}
Import-Module $commonPath -Force

$repoRoot = (Resolve-Path (Join-Path $scriptDir '..')).ProviderPath
$kernelDir = Join-Path $repoRoot 'r0/driver/msbuild'
$outputDir = Join-Path $kernelDir "out\$Platform\$Configuration"
$driverSys = Join-Path $outputDir 'FclMusaDriver.sys'
$driverPdb = Join-Path $outputDir 'FclMusaDriver.pdb'
$distDir = Join-Path $repoRoot "dist\driver\$Platform\$Configuration"

# 步骤 0: 准备依赖
Write-Host "Setting up dependencies..." -ForegroundColor Cyan
$setupDepsScript = Join-Path $scriptDir 'setup_dependencies.ps1'
& $setupDepsScript
if ($LASTEXITCODE -ne 0) {
    throw "Dependency setup failed."
}

Ensure-MusaRuntimePublish -RepoRoot $repoRoot

Write-Host "[1/3] Building driver solution ($Configuration|$Platform)..." -ForegroundColor Cyan
$previousRuntimeOverride = $env:MUSA_RUNTIME_LIBRARY_CONFIGURATION
$runtimeOverrideApplied = $false
if ($Configuration -eq 'Debug') {
    $env:MUSA_RUNTIME_LIBRARY_CONFIGURATION = 'Release'
    $runtimeOverrideApplied = $true
}

try {
    & (Join-Path $scriptDir 'manual_build.cmd') $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "manual_build.cmd failed with exit code $LASTEXITCODE. See r0\\driver\\msbuild\\build_manual_build.log for details."
    }
}
finally {
    if ($runtimeOverrideApplied) {
        if ([string]::IsNullOrWhiteSpace($previousRuntimeOverride)) {
            Remove-Item Env:MUSA_RUNTIME_LIBRARY_CONFIGURATION -ErrorAction SilentlyContinue
        } else {
            $env:MUSA_RUNTIME_LIBRARY_CONFIGURATION = $previousRuntimeOverride
        }
    }
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
Ensure-Directory $distDir

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
