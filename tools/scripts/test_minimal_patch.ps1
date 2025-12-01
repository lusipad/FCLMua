<#
.SYNOPSIS
    测试最小化补丁文件的有效性
#>

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$fclSourcePath = Join-Path $repoRoot 'external\fcl-source'
$minimalPatchPath = Join-Path $repoRoot 'patches\fcl-kernel-mode-minimal.patch'

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  测试最小化补丁" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $minimalPatchPath)) {
    throw "未找到最小化补丁文件: $minimalPatchPath"
}

Write-Host "1. 恢复 fcl-source 到干净状态..." -ForegroundColor Yellow
& (Join-Path $repoRoot 'tools\scripts\apply_fcl_patch.ps1') -Restore -Quiet
Write-Host "   ✓ 已恢复" -ForegroundColor Green

Write-Host ""
Write-Host "2. 测试补丁是否可以应用..." -ForegroundColor Yellow
Push-Location $fclSourcePath
try {
    $checkResult = & git apply --check $minimalPatchPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "   ✗ 补丁检查失败" -ForegroundColor Red
        Write-Host $checkResult -ForegroundColor Red
        throw "补丁无法应用"
    }
    Write-Host "   ✓ 补丁格式正确" -ForegroundColor Green

    Write-Host ""
    Write-Host "3. 应用补丁..." -ForegroundColor Yellow
    $applyResult = & git apply $minimalPatchPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "   ✗ 应用失败" -ForegroundColor Red
        Write-Host $applyResult -ForegroundColor Red
        throw "补丁应用失败"
    }
    Write-Host "   ✓ 应用成功" -ForegroundColor Green

    Write-Host ""
    Write-Host "4. 检查修改的文件..." -ForegroundColor Yellow
    $modifiedFiles = @(git status --short | ForEach-Object { $_.Substring(3) })
    Write-Host "   修改文件数: $($modifiedFiles.Count)" -ForegroundColor Gray

    $sourceFiles = $modifiedFiles | Where-Object { $_ -match '^(include|src)/' }
    Write-Host "   源代码文件: $($sourceFiles.Count)" -ForegroundColor Green

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "  ✓ 测试通过！补丁可以正常使用" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
}
finally {
    # 恢复干净状态
    & git reset --hard HEAD 2>&1 | Out-Null
    & git clean -fd 2>&1 | Out-Null
    Pop-Location
}
