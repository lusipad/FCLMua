<#
.SYNOPSIS
    文档生成脚本

.DESCRIPTION
    生成项目文档
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$script:DocsDir = Join-Path $script:RepoRoot 'docs'

function Write-TaskHeader {
    param([string]$Title)
    Write-Host "`n============================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "============================================`n" -ForegroundColor Cyan
}

Write-TaskHeader "生成项目文档"

# 检查是否有 Doxygen 或其他文档工具
$doxygenPath = Get-Command doxygen -ErrorAction SilentlyContinue

if ($doxygenPath) {
    Write-Host "发现 Doxygen: $($doxygenPath.Source)" -ForegroundColor Green
    
    $doxyfile = Join-Path $script:DocsDir 'Doxyfile'
    
    if (Test-Path $doxyfile) {
        Write-Host "运行 Doxygen..." -ForegroundColor Yellow
        Push-Location $script:DocsDir
        try {
            & doxygen Doxyfile
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "`n✓ 文档生成成功" -ForegroundColor Green
                
                $htmlIndex = Join-Path $script:DocsDir 'html\index.html'
                if (Test-Path $htmlIndex) {
                    Write-Host "文档位置: $htmlIndex" -ForegroundColor Cyan
                }
            } else {
                Write-Host "`n✗ 文档生成失败" -ForegroundColor Red
            }
        }
        finally {
            Pop-Location
        }
    } else {
        Write-Host "✗ 未找到 Doxyfile 配置文件" -ForegroundColor Red
        Write-Host "  预期位置: $doxyfile" -ForegroundColor Yellow
    }
} else {
    Write-Host "未安装 Doxygen" -ForegroundColor Yellow
    Write-Host "`n当前可用的文档:" -ForegroundColor Cyan
    
    $mdFiles = Get-ChildItem -Path $script:RepoRoot -Filter "*.md" | Select-Object -First 10
    foreach ($file in $mdFiles) {
        Write-Host "  - $($file.Name)" -ForegroundColor Gray
    }
    
    if (Test-Path $script:DocsDir) {
        Write-Host "`ndocs 目录内容:" -ForegroundColor Cyan
        Get-ChildItem -Path $script:DocsDir -Recurse -File | ForEach-Object {
            Write-Host "  - $($_.FullName.Replace($script:RepoRoot, '.'))" -ForegroundColor Gray
        }
    }
}

Write-Host "`n✓ 文档任务完成" -ForegroundColor Green
