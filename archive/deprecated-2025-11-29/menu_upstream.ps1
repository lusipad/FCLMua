<#
.SYNOPSIS
    上游检查脚本

.DESCRIPTION
    检查上游仓库更新
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$script:ToolsDir = Join-Path $script:RepoRoot 'tools'

function Write-TaskHeader {
    param([string]$Title)
    Write-Host "`n============================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "============================================`n" -ForegroundColor Cyan
}

Write-TaskHeader "检查上游仓库更新"

Push-Location $script:RepoRoot
try {
    # 检查是否在 git 仓库中
    $isGitRepo = Test-Path (Join-Path $script:RepoRoot '.git')
    
    if (-not $isGitRepo) {
        Write-Host "✗ 当前目录不是 Git 仓库" -ForegroundColor Red
        exit 1
    }
    
    # 显示当前远程仓库
    Write-Host "当前远程仓库配置:" -ForegroundColor Yellow
    git remote -v
    
    # 获取远程更新
    Write-Host "`n正在获取远程更新..." -ForegroundColor Yellow
    git fetch --all --prune
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "✗ 获取远程更新失败" -ForegroundColor Red
        exit 1
    }
    
    # 检查当前分支
    $currentBranch = git rev-parse --abbrev-ref HEAD
    Write-Host "`n当前分支: $currentBranch" -ForegroundColor Cyan
    
    # 检查是否有上游更新
    $upstream = "origin/$currentBranch"
    
    $ahead = git rev-list --count HEAD..$upstream 2>$null
    $behind = git rev-list --count $upstream..HEAD 2>$null
    
    if ($ahead -and $behind) {
        Write-Host "`n分支状态:" -ForegroundColor Yellow
        
        if ([int]$ahead -gt 0) {
            Write-Host "  ⬇ 落后上游 $ahead 个提交" -ForegroundColor Yellow
        }
        
        if ([int]$behind -gt 0) {
            Write-Host "  ⬆ 领先上游 $behind 个提交" -ForegroundColor Green
        }
        
        if ([int]$ahead -eq 0 -and [int]$behind -eq 0) {
            Write-Host "  ✓ 与上游同步" -ForegroundColor Green
        }
        
        # 显示最新的上游提交
        if ([int]$ahead -gt 0) {
            Write-Host "`n上游最新提交:" -ForegroundColor Cyan
            git log --oneline -5 $upstream
            
            Write-Host "`n提示: 使用 'git pull' 拉取更新" -ForegroundColor Yellow
        }
    } else {
        Write-Host "`n未配置上游分支或分支不存在" -ForegroundColor Yellow
    }
    
    # 检查子模块
    Write-Host "`n检查子模块..." -ForegroundColor Yellow
    $submodules = git submodule status
    
    if ($submodules) {
        Write-Host "子模块状态:" -ForegroundColor Cyan
        Write-Host $submodules
        
        Write-Host "`n更新子模块..." -ForegroundColor Yellow
        git submodule update --init --recursive
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ 子模块已更新" -ForegroundColor Green
        }
    } else {
        Write-Host "未使用子模块" -ForegroundColor Gray
    }
    
    # 使用现有的验证脚本（如果存在）
    $verifyScript = Join-Path $script:ToolsDir 'verify_upstream.ps1'
    if (Test-Path $verifyScript) {
        Write-Host "`n运行上游验证脚本..." -ForegroundColor Yellow
        try {
            & $verifyScript
        } catch {
            Write-Host "⚠ 验证脚本失败: $_" -ForegroundColor Yellow
        }
    }
    
    Write-Host "`n✓ 上游检查完成" -ForegroundColor Green
}
catch {
    Write-Host "`n✗ 上游检查失败: $_" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
