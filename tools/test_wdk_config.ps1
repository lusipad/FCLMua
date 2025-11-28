<#
.SYNOPSIS
    测试 WDK 版本配置功能

.DESCRIPTION
    验证所有 WDK 版本配置方式是否正常工作
#>

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "FCL+Musa WDK Version Configuration Test" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# 测试 1: 检测系统上可用的 WDK 版本
Write-Host "[Test 1] Detecting available WDK versions..." -ForegroundColor Yellow
$WINDOWS_KITS_ROOT = "${env:ProgramFiles(x86)}\Windows Kits\10"
$availableVersions = @()

Get-ChildItem "$WINDOWS_KITS_ROOT\Include" -Directory | ForEach-Object {
    $ntddk = Join-Path $_.FullName "km\ntddk.h"
    if (Test-Path $ntddk) {
        $availableVersions += $_.Name
        Write-Host "  ✓ Found: $($_.Name)" -ForegroundColor Green
    }
}

if ($availableVersions.Count -eq 0) {
    Write-Host "  ✗ No WDK installations found!" -ForegroundColor Red
    exit 1
}
Write-Host ""

# 测试 2: 读取配置文件
Write-Host "[Test 2] Testing config file parsing..." -ForegroundColor Yellow
$configExample = Join-Path $scriptDir "build.config.example"
if (Test-Path $configExample) {
    Write-Host "  ✓ build.config.example exists" -ForegroundColor Green
} else {
    Write-Host "  ✗ build.config.example not found!" -ForegroundColor Red
}

$buildConfig = Join-Path $scriptDir "build.config"
if (Test-Path $buildConfig) {
    Write-Host "  ✓ build.config exists (user configured)" -ForegroundColor Green

    # 尝试解析
    $config = @{}
    Get-Content $buildConfig | ForEach-Object {
        $line = $_.Trim()
        if ($line -and -not $line.StartsWith('#')) {
            if ($line -match '^(\w+)\s*=\s*(.+)$') {
                $config[$matches[1]] = $matches[2].Trim()
                Write-Host "    - $($matches[1]) = $($matches[2].Trim())" -ForegroundColor Gray
            }
        }
    }
} else {
    Write-Host "  ℹ build.config not found (will use auto-detection)" -ForegroundColor Gray
}
Write-Host ""

# 测试 3: 检查环境变量
Write-Host "[Test 3] Checking environment variable..." -ForegroundColor Yellow
if ($env:FCL_MUSA_WDK_VERSION) {
    Write-Host "  ✓ FCL_MUSA_WDK_VERSION = $env:FCL_MUSA_WDK_VERSION" -ForegroundColor Green
} else {
    Write-Host "  ℹ FCL_MUSA_WDK_VERSION not set" -ForegroundColor Gray
}
Write-Host ""

# 测试 4: 验证构建脚本
Write-Host "[Test 4] Verifying build scripts..." -ForegroundColor Yellow
$buildAll = Join-Path $scriptDir "build_all.ps1"
$buildSimplified = Join-Path $scriptDir "build_simplified.ps1"

if (Test-Path $buildAll) {
    # 检查是否包含 WdkVersion 参数
    $content = Get-Content $buildAll -Raw
    if ($content -match '\[string\]\$WdkVersion') {
        Write-Host "  ✓ build_all.ps1 supports -WdkVersion parameter" -ForegroundColor Green
    } else {
        Write-Host "  ✗ build_all.ps1 missing -WdkVersion parameter!" -ForegroundColor Red
    }
} else {
    Write-Host "  ✗ build_all.ps1 not found!" -ForegroundColor Red
}

if (Test-Path $buildSimplified) {
    $content = Get-Content $buildSimplified -Raw
    if ($content -match '\[string\]\$WdkVersion') {
        Write-Host "  ✓ build_simplified.ps1 supports -WdkVersion parameter" -ForegroundColor Green
    } else {
        Write-Host "  ✗ build_simplified.ps1 missing -WdkVersion parameter!" -ForegroundColor Red
    }
} else {
    Write-Host "  ✗ build_simplified.ps1 not found!" -ForegroundColor Red
}
Write-Host ""

# 测试 5: 模拟不同配置方式
Write-Host "[Test 5] Simulating configuration priorities..." -ForegroundColor Yellow

# 优先级测试
$testVersion = "10.0.26100.0"

Write-Host "  Scenario 1: Command line parameter (highest priority)" -ForegroundColor Cyan
Write-Host "    Command: .\build_all.ps1 -WdkVersion $testVersion" -ForegroundColor Gray
Write-Host "    Expected: Uses $testVersion" -ForegroundColor Gray

Write-Host ""
Write-Host "  Scenario 2: Environment variable" -ForegroundColor Cyan
Write-Host "    Command: `$env:FCL_MUSA_WDK_VERSION='$testVersion'; .\build_all.ps1" -ForegroundColor Gray
Write-Host "    Expected: Uses $testVersion" -ForegroundColor Gray

Write-Host ""
Write-Host "  Scenario 3: Config file" -ForegroundColor Cyan
Write-Host "    File: tools\build.config with WdkVersion=$testVersion" -ForegroundColor Gray
Write-Host "    Command: .\build_all.ps1" -ForegroundColor Gray
Write-Host "    Expected: Uses $testVersion" -ForegroundColor Gray

Write-Host ""
Write-Host "  Scenario 4: Auto-detection (lowest priority)" -ForegroundColor Cyan
Write-Host "    Command: .\build_all.ps1 (no config)" -ForegroundColor Gray
Write-Host "    Expected: Uses first available: $($availableVersions[0])" -ForegroundColor Gray

Write-Host ""

# 总结
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Test Summary" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Available WDK Versions: $($availableVersions.Count)" -ForegroundColor Green
Write-Host "Recommended Version:    10.0.22621.0" -ForegroundColor Gray
Write-Host "Auto-detected Version:  $($availableVersions[0])" -ForegroundColor Gray
Write-Host ""
Write-Host "To build with specific WDK version:" -ForegroundColor Cyan
Write-Host "  .\tools\build_all.ps1 -WdkVersion $($availableVersions[0])" -ForegroundColor White
Write-Host ""
Write-Host "For more information, see:" -ForegroundColor Cyan
Write-Host "  docs\wdk_version_config.md" -ForegroundColor White
Write-Host ""
