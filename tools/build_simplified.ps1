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

    [string]$WdkVersion = $null,

    [string]$WdkRoot = $null
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

# 计算 WDK 探测参数（优先级：命令行 > 环境变量 > 配置文件）
$wdkParams = @{}
if ($WdkVersion) {
    $wdkParams['RequestedVersion'] = $WdkVersion
} elseif ($env:FCL_MUSA_WDK_VERSION) {
    $wdkParams['RequestedVersion'] = $env:FCL_MUSA_WDK_VERSION
} elseif ($buildConfig.ContainsKey('WdkVersion')) {
    $wdkParams['RequestedVersion'] = $buildConfig['WdkVersion']
}

if ($WdkRoot) {
    $wdkParams['RequestedRoot'] = $WdkRoot
} elseif ($env:FCL_MUSA_WDK_ROOT) {
    $wdkParams['RequestedRoot'] = $env:FCL_MUSA_WDK_ROOT
} elseif ($buildConfig.ContainsKey('WdkRoot')) {
    $wdkParams['RequestedRoot'] = $buildConfig['WdkRoot']
} elseif ($env:WDKContentRoot) {
    $wdkParams['RequestedRoot'] = $env:WDKContentRoot
}

# Auto-detect WDK root/version
Write-Host "Detecting WDK installation..." -ForegroundColor Cyan
$wdkInfo = Resolve-WdkEnvironment @wdkParams
$WDK_VERSION = $wdkInfo.Version
Write-Host "  Using WDK $($wdkInfo.Version) (root: $($wdkInfo.Root))" -ForegroundColor Green

# Set environment variables for MSBuild
if ($wdkInfo.IncludePaths -and $wdkInfo.IncludePaths.Count -gt 0) {
    $env:INCLUDE = ($wdkInfo.IncludePaths -join ';') + ';' + $env:INCLUDE
}
if ($wdkInfo.LibPaths -and $wdkInfo.LibPaths.Count -gt 0) {
    $env:LIB = ($wdkInfo.LibPaths -join ';') + ';' + $env:LIB
}
if ($wdkInfo.BinPaths -and $wdkInfo.BinPaths.Count -gt 0) {
    $env:PATH = ($wdkInfo.BinPaths -join ';') + ';' + $env:PATH
}

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
