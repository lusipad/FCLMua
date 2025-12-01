<#
.SYNOPSIS
    测试 WDK 自动检测功能
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = $PSScriptRoot
Import-Module (Join-Path $RepoRoot 'tools\build\common.psm1') -Force

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  测试 WDK 自动检测功能" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

try {
    Write-Host "正在检测 WDK..." -ForegroundColor Yellow
    $wdk = Find-FCLWDK

    Write-Host ""
    Write-Host "✓ WDK 检测成功！" -ForegroundColor Green
    Write-Host ""
    Write-Host "检测到的 WDK 信息：" -ForegroundColor Cyan
    Write-Host "  根目录: $($wdk.Root)" -ForegroundColor Gray
    Write-Host "  版本:   $($wdk.Version)" -ForegroundColor Green
    Write-Host "  头文件: $($wdk.IncludePath)" -ForegroundColor Gray
    Write-Host "  库路径: $($wdk.LibPath)" -ForegroundColor Gray
    Write-Host ""

    # 验证关键路径
    $kmPath = Join-Path $wdk.IncludePath 'km'
    if (Test-Path $kmPath) {
        Write-Host "✓ 内核模式头文件目录存在" -ForegroundColor Green
    } else {
        Write-Host "✗ 内核模式头文件目录不存在：$kmPath" -ForegroundColor Red
        exit 1
    }

    $libPath = Join-Path $wdk.LibPath "km\x64"
    if (Test-Path $libPath) {
        Write-Host "✓ 内核模式库目录存在" -ForegroundColor Green
    } else {
        Write-Host "⚠ 内核模式库目录不存在：$libPath" -ForegroundColor Yellow
    }

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "  测试通过！build.ps1 将使用此版本" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
}
catch {
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Red
    Write-Host "  ✗ WDK 检测失败" -ForegroundColor Red
    Write-Host "============================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "错误: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "请确保已安装 Windows Driver Kit (WDK)" -ForegroundColor Yellow
    exit 1
}
