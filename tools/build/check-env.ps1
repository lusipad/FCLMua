<#
.SYNOPSIS
    检查构建环境
#>

$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$script:RepoRoot = Get-FCLRepoRoot

Write-FCLHeader "检查构建环境"

$allGood = $true

# Check Visual Studio
Write-Host "[1/4] Checking Visual Studio..." -ForegroundColor Cyan
try {
    $vsDevCmd = Find-FCLVsDevCmd
    Write-Host "  ✓ Found: $vsDevCmd" -ForegroundColor Green
}
catch {
    Write-Host "  ✗ Visual Studio not found" -ForegroundColor Red
    Write-Host "    $_" -ForegroundColor Gray
    $allGood = $false
}

# Check MSBuild
Write-Host ""
Write-Host "[2/4] Checking MSBuild..." -ForegroundColor Cyan
try {
    $msbuild = Find-FCLMSBuild
    Write-Host "  ✓ Found: $msbuild" -ForegroundColor Green
    
    $version = & $msbuild /version /nologo | Select-Object -Last 1
    Write-Host "    Version: $version" -ForegroundColor Gray
}
catch {
    Write-Host "  ✗ MSBuild not found" -ForegroundColor Red
    Write-Host "    $_" -ForegroundColor Gray
    $allGood = $false
}

# Check WDK
Write-Host ""
Write-Host "[3/4] Checking Windows Driver Kit (WDK)..." -ForegroundColor Cyan
try {
    $wdk = Find-FCLWDK
    Write-Host "  ✓ WDK Version: $($wdk.Version)" -ForegroundColor Green
    Write-Host "    Root: $($wdk.Root)" -ForegroundColor Gray
}
catch {
    Write-Host "  ✗ WDK not found" -ForegroundColor Red
    Write-Host "    $_" -ForegroundColor Gray
    $allGood = $false
}

# Check CMake
Write-Host ""
Write-Host "[4/4] Checking CMake..." -ForegroundColor Cyan
try {
    $cmakeVersion = & cmake --version 2>$null | Select-Object -First 1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ $cmakeVersion" -ForegroundColor Green
    }
    else {
        throw "CMake not found in PATH"
    }
}
catch {
    Write-Host "  ✗ CMake not found" -ForegroundColor Red
    Write-Host "    $_" -ForegroundColor Gray
    $allGood = $false
}

# Check Musa.Runtime
Write-Host ""
Write-Host "[5/5] Checking Musa.Runtime..." -ForegroundColor Cyan
$publishDir = Join-Path $script:RepoRoot 'external\Musa.Runtime\Publish'
$versionFile = Join-Path $publishDir '.version'

if (Test-Path $versionFile) {
    $version = Get-Content $versionFile -Raw
    Write-Host "  ✓ Installed: $($version.Trim())" -ForegroundColor Green
}
else {
    Write-Host "  ⚠ Not installed (will be downloaded on first build)" -ForegroundColor Yellow
}

Write-Host ""
if ($allGood) {
    Write-FCLSuccess "环境检查完成 - 所有依赖已就绪"
}
else {
    Write-FCLError "环境检查失败 - 请安装缺失的依赖"
    exit 1
}
