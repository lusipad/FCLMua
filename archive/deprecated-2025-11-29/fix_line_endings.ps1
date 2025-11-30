<#
.SYNOPSIS
    修复 Windows 脚本文件的换行符

.DESCRIPTION
    将 Unix 换行符 (LF) 转换为 Windows 换行符 (CRLF)
    主要用于 .cmd 和 .bat 文件
#>

param(
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$toolsDir = Join-Path $repoRoot 'tools'

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  修复 Windows 脚本换行符" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if ($DryRun) {
    Write-Host "模式: 预览（不实际修改文件）" -ForegroundColor Yellow
} else {
    Write-Host "模式: 执行修复" -ForegroundColor Green
}

Write-Host ""

# 查找所有需要修复的文件
$patterns = @('*.cmd', '*.bat')
$fixedCount = 0
$skippedCount = 0

foreach ($pattern in $patterns) {
    Write-Host "检查 $pattern 文件..." -ForegroundColor Yellow
    
    $files = Get-ChildItem -Path $repoRoot -Filter $pattern -Recurse -File
    
    foreach ($file in $files) {
        $relativePath = $file.FullName.Replace($repoRoot, '.').TrimStart('\')
        
        # 读取文件内容
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        
        if (-not $content) {
            Write-Host "  ⊗ 跳过空文件: $relativePath" -ForegroundColor DarkGray
            $skippedCount++
            continue
        }
        
        # 检查是否有 LF 但没有 CRLF
        $hasLF = $content -match "`n"
        $hasCRLF = $content -match "`r`n"
        
        if ($hasLF -and -not $hasCRLF) {
            # 需要修复
            if ($DryRun) {
                Write-Host "  [DRY RUN] 将修复: $relativePath" -ForegroundColor Yellow
            } else {
                # 替换 LF 为 CRLF
                $fixedContent = $content -replace "`n", "`r`n"
                
                # 写回文件（使用 ASCII 编码）
                [System.IO.File]::WriteAllText($file.FullName, $fixedContent, [System.Text.Encoding]::ASCII)
                
                Write-Host "  ✓ 已修复: $relativePath" -ForegroundColor Green
            }
            $fixedCount++
        } else {
            Write-Host "  ✓ 正常: $relativePath" -ForegroundColor Gray
        }
    }
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan

if ($DryRun) {
    Write-Host "  预览完成！" -ForegroundColor Yellow
    Write-Host "  发现 $fixedCount 个文件需要修复" -ForegroundColor Yellow
    Write-Host "  使用不带 -DryRun 参数执行实际修复" -ForegroundColor Yellow
} else {
    Write-Host "  修复完成！" -ForegroundColor Green
    Write-Host "  修复了 $fixedCount 个文件" -ForegroundColor Green
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if ($fixedCount -gt 0) {
    Write-Host "提示: 修复的文件现在应该可以在 Windows 上正常运行" -ForegroundColor Cyan
}
