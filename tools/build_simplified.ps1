<#
.SYNOPSIS
    Simplified build script with auto-detection of WDK version

.PARAMETER Configuration
    Build configuration: Debug or Release (default: Debug)

.PARAMETER Platform
    Target platform: x64 (default: x64)

.PARAMETER WdkVersion
    WDK version to use (e.g., 10.0.22621.0, 10.0.26100.0)
    If not specified, will auto-detect from available installations

.EXAMPLE
    .\build_simplified.ps1
    Build with auto-detected WDK version

.EXAMPLE
    .\build_simplified.ps1 -Configuration Release -WdkVersion 10.0.22621.0
    Build Release with specific WDK version
#>

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [string]$WdkVersion = $null
)

$ErrorActionPreference = 'Stop'

# 读取配置文件（如果存在）
function Read-BuildConfig {
    param([string]$ConfigFile)
    $config = @{}
    if (Test-Path $ConfigFile) {
        Get-Content $ConfigFile | ForEach-Object {
            $line = $_.Trim()
            if ($line -and -not $line.StartsWith('#')) {
                if ($line -match '^(\w+)\s*=\s*(.+)$') {
                    $config[$matches[1]] = $matches[2].Trim()
                }
            }
        }
    }
    return $config
}

# Import MSBuild discovery
$scriptDir = $PSScriptRoot
Import-Module (Join-Path $scriptDir 'common.psm1') -Force

# Get repository paths
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..')).ProviderPath
$kernelDir = Join-Path $repoRoot 'kernel\FclMusaDriver'
$solutionFile = Join-Path $kernelDir 'FclMusaDriver.sln'

# 读取构建配置文件
$buildConfig = Read-BuildConfig -ConfigFile (Join-Path $scriptDir 'build.config')

# 应用 WDK 版本配置（优先级：命令行 > 环境变量 > 配置文件）
if (-not $WdkVersion) {
    if ($env:FCL_MUSA_WDK_VERSION) {
        $WdkVersion = $env:FCL_MUSA_WDK_VERSION
    } elseif ($buildConfig.ContainsKey('WdkVersion')) {
        $WdkVersion = $buildConfig['WdkVersion']
    }
}

Write-Host "Building FCL+Musa Driver..." -ForegroundColor Cyan
Write-Host "Configuration: $Configuration | $Platform" -ForegroundColor Gray
Write-Host ""

# Ensure Musa.Runtime is ready
Write-Host "Checking Musa.Runtime..." -ForegroundColor Cyan
Ensure-MusaRuntimePublish -RepoRoot $repoRoot
Write-Host "  OK" -ForegroundColor Green

# Find MSBuild
Write-Host "Locating MSBuild..." -ForegroundColor Cyan
$msbuild = Get-MSBuildPath
Write-Host "  Found: $msbuild" -ForegroundColor Green

# Auto-detect WDK version
$WINDOWS_KITS_ROOT = "${env:ProgramFiles(x86)}\Windows Kits\10"

if ($WdkVersion) {
    # User specified WDK version
    Write-Host "Using specified WDK version: $WdkVersion" -ForegroundColor Cyan
    $testHeader = "$WINDOWS_KITS_ROOT\Include\$WdkVersion\km\ntddk.h"
    if (-not (Test-Path $testHeader)) {
        throw "Specified WDK version $WdkVersion not found or incomplete. Please check installation."
    }
    $WDK_VERSION = $WdkVersion
} else {
    # Auto-detect from available versions
    $WDK_VERSIONS = @('10.0.22621.0', '10.0.26100.0', '10.0.22000.0')
    $WDK_VERSION = $null

    Write-Host "Detecting WDK installation..." -ForegroundColor Cyan
    foreach ($ver in $WDK_VERSIONS) {
        $testHeader = "$WINDOWS_KITS_ROOT\Include\$ver\km\ntddk.h"
        if (Test-Path $testHeader) {
            $WDK_VERSION = $ver
            Write-Host "  Found WDK $WDK_VERSION" -ForegroundColor Green
            break
        }
    }

    if (-not $WDK_VERSION) {
        throw "WDK not found. Tested versions: $($WDK_VERSIONS -join ', '). Please install Windows Driver Kit or specify version with -WdkVersion parameter."
    }
}

$WDK_INCLUDE = "$WINDOWS_KITS_ROOT\Include\$WDK_VERSION"
$WDK_LIB = "$WINDOWS_KITS_ROOT\Lib\$WDK_VERSION"

# Set environment variables for MSBuild
$env:INCLUDE = "$WDK_INCLUDE\km;$WDK_INCLUDE\shared;$WDK_INCLUDE\ucrt;$WDK_INCLUDE\um;$env:INCLUDE"
$env:LIB = "$WDK_LIB\km\x64;$WDK_LIB\um\x64;$env:LIB"
$env:PATH = "$WINDOWS_KITS_ROOT\bin\$WDK_VERSION\x64;$env:PATH"

# Prepare MSBuild properties
$msbuildProps = @(
    "/p:Configuration=$Configuration"
    "/p:Platform=$Platform"
    "/p:WindowsTargetPlatformVersion=$WDK_VERSION"
    "/p:TargetVersion=Windows10"
)

# Clean
Write-Host ""
Write-Host "Cleaning previous build..." -ForegroundColor Cyan
& $msbuild $solutionFile `
    /t:Clean `
    @msbuildProps `
    /v:minimal `
    /nologo

if ($LASTEXITCODE -ne 0) {
    throw "Clean failed"
}
Write-Host "  OK" -ForegroundColor Green

# Build
Write-Host ""
Write-Host "Building solution..." -ForegroundColor Cyan
& $msbuild $solutionFile `
    /t:Build `
    @msbuildProps `
    /m `
    /v:normal `
    /nologo

if ($LASTEXITCODE -ne 0) {
    throw "Build failed"
}

# Check output
$outputDir = Join-Path $kernelDir "out\$Platform\$Configuration"
$driverSys = Join-Path $outputDir 'FclMusaDriver.sys'

if (Test-Path $driverSys) {
    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "Driver: $driverSys" -ForegroundColor Gray
} else {
    throw "Driver file not found: $driverSys"
}
