<#
.SYNOPSIS
    清理旧构建脚本

.DESCRIPTION
    将已被新菜单系统替代的旧脚本移动到 archive/ 目录
#>

param(
    [switch]$DryRun,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$archiveDir = Join-Path $repoRoot 'archive\old-build-scripts'

# 确保 archive 目录存在
if (-not $DryRun) {
    if (-not (Test-Path $archiveDir)) {
        New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null
        Write-Host "创建归档目录: $archiveDir" -ForegroundColor Cyan
    }
}

# 要移动的文件列表
$filesToArchive = @{
    # 主目录
    'Root' = @(
        # 注意：build.ps1 已被 menu.ps1 替代，但可能不存在
    )
    
    # tools 目录
    'Tools' = @(
        'build_interactive.ps1'     # 被 menu.ps1 替代
        'build_simplified.ps1'      # 被 menu_build.ps1 替代
        'build_all_backup.ps1'      # 备份文件
        'build_all_fixed.ps1'       # 临时文件
        'run_all_tests.ps1'         # 被 menu_test.ps1 替代
        'manual_build.cmd'          # 被 manual_build.ps1 替代（PowerShell版本）
    )
}

function Move-FileToArchive {
    param(
        [string]$SourcePath,
        [string]$RelativePath,
        [bool]$IsDryRun
    )
    
    if (-not (Test-Path $SourcePath)) {
        Write-Host "  ⊗ 文件不存在: $RelativePath" -ForegroundColor DarkGray
        return
    }
    
    $archivePath = Join-Path $archiveDir (Split-Path -Leaf $SourcePath)
    
    if ($IsDryRun) {
        Write-Host "  [DRY RUN] 将移动: $RelativePath → archive/" -ForegroundColor Yellow
    } else {
        try {
            Move-Item -Path $SourcePath -Destination $archivePath -Force
            Write-Host "  ✓ 已移动: $RelativePath" -ForegroundColor Green
        }
        catch {
            Write-Host "  ✗ 移动失败: $RelativePath - $_" -ForegroundColor Red
        }
    }
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  清理旧构建脚本" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if ($DryRun) {
    Write-Host "模式: 预览（不实际移动文件）" -ForegroundColor Yellow
} else {
    Write-Host "模式: 执行移动" -ForegroundColor Green
}

Write-Host ""
Write-Host "归档目录: $archiveDir" -ForegroundColor Cyan
Write-Host ""

# 处理主目录文件
Write-Host "主目录文件:" -ForegroundColor Yellow
foreach ($file in $filesToArchive['Root']) {
    $sourcePath = Join-Path $repoRoot $file
    Move-FileToArchive -SourcePath $sourcePath -RelativePath $file -IsDryRun $DryRun
}

Write-Host ""

# 处理 tools 目录文件
Write-Host "Tools 目录文件:" -ForegroundColor Yellow
$toolsDir = Join-Path $repoRoot 'tools'
foreach ($file in $filesToArchive['Tools']) {
    $sourcePath = Join-Path $toolsDir $file
    $relativePath = "tools\$file"
    Move-FileToArchive -SourcePath $sourcePath -RelativePath $relativePath -IsDryRun $DryRun
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan

if ($DryRun) {
    Write-Host "  预览完成！" -ForegroundColor Yellow
    Write-Host "  使用 -DryRun:$false 执行实际移动" -ForegroundColor Yellow
} else {
    Write-Host "  清理完成！" -ForegroundColor Green
    Write-Host "  旧脚本已移至: $archiveDir" -ForegroundColor Green
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 显示保留的关键脚本
Write-Host "保留的关键脚本（被新系统调用）:" -ForegroundColor Cyan
$keepScripts = @(
    'tools\manual_build.cmd',
    'tools\sign_driver.ps1',
    'tools\setup_dependencies.ps1',
    'tools\package_bundle.ps1',
    'tools\fcl-self-test.ps1',
    'tools\verify_upstream.ps1',
    'tools\common.psm1'
)

foreach ($script in $keepScripts) {
    $scriptPath = Join-Path $repoRoot $script
    if (Test-Path $scriptPath) {
        Write-Host "  ✓ $script" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "新菜单系统脚本:" -ForegroundColor Cyan
$newScripts = @(
    'menu.ps1',
    'tools\menu_build.ps1',
    'tools\menu_test.ps1',
    'tools\menu_doc.ps1',
    'tools\menu_checkenv.ps1',
    'tools\menu_upstream.ps1'
)

foreach ($script in $newScripts) {
    $scriptPath = Join-Path $repoRoot $script
    if (Test-Path $scriptPath) {
        Write-Host "  ✓ $script" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $script (缺失)" -ForegroundColor Red
    }
}

Write-Host ""

if (-not $DryRun) {
    Write-Host "提示: 旧脚本已归档，如需恢复可从 archive/ 目录找回" -ForegroundColor Gray
}
