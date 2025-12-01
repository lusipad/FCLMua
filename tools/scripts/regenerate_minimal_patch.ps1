<#
.SYNOPSIS
    重新生成最小化的 FCL 内核模式补丁文件

.DESCRIPTION
    只包含必要的源代码修改，排除 CI/构建系统/文档等文件
    大幅减小补丁文件大小，便于代码审查
#>

param(
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$fclSourcePath = Join-Path $repoRoot 'external\fcl-source'
$patchPath = Join-Path $repoRoot 'patches\fcl-kernel-mode.patch'
$minimalPatchPath = Join-Path $repoRoot 'patches\fcl-kernel-mode-minimal.patch'

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  重新生成最小化 FCL 内核模式补丁" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 确保 fcl-source 是 Git 仓库
if (-not (Test-Path (Join-Path $fclSourcePath '.git'))) {
    throw "external/fcl-source 不是有效的 Git 子模块"
}

# 检查当前状态
Write-Host "检查当前修改状态..." -ForegroundColor Yellow
Push-Location $fclSourcePath
try {
    $allModified = @(git status --short | ForEach-Object { $_.Substring(3) })
    $totalCount = $allModified.Count

    Write-Host "  总修改文件数: $totalCount" -ForegroundColor Gray

    # 定义需要包含的路径模式
    $includePatterns = @(
        '^include/',
        '^src/'
    )

    # 定义需要排除的路径模式
    $excludePatterns = @(
        '\.github/',
        '\.travis',
        'appveyor',
        'CMakeLists\.txt$',
        'CMakeModules/',
        '\.md$',
        '\.txt$',
        '\.cmake$',
        '\.sh$',
        '\.yml$',
        '\.yaml$',
        'test/',
        'example/'
    )

    # 筛选文件
    $filteredFiles = $allModified | Where-Object {
        $file = $_
        $include = $false

        # 检查是否匹配包含模式
        foreach ($pattern in $includePatterns) {
            if ($file -match $pattern) {
                $include = $true
                break
            }
        }

        # 如果不在包含列表中，直接排除
        if (-not $include) {
            return $false
        }

        # 检查是否匹配排除模式
        foreach ($pattern in $excludePatterns) {
            if ($file -match $pattern) {
                return $false
            }
        }

        return $true
    }

    $filteredCount = $filteredFiles.Count
    $excludedCount = $totalCount - $filteredCount

    Write-Host ""
    Write-Host "筛选结果：" -ForegroundColor Cyan
    Write-Host "  包含文件: $filteredCount" -ForegroundColor Green
    Write-Host "  排除文件: $excludedCount" -ForegroundColor Yellow
    Write-Host ""

    if ($filteredCount -eq 0) {
        throw "没有找到需要打包的源代码修改！"
    }

    # 显示包含的文件
    Write-Host "将包含以下文件：" -ForegroundColor Cyan
    $filteredFiles | Sort-Object | ForEach-Object {
        Write-Host "  $_" -ForegroundColor Gray
    }
    Write-Host ""

    if ($DryRun) {
        Write-Host "DryRun 模式，不生成补丁文件" -ForegroundColor Yellow
        return
    }

    # 生成最小化补丁
    Write-Host "正在从原补丁筛选文件..." -ForegroundColor Yellow

    # 读取原补丁内容
    Pop-Location
    $originalPatch = Get-Content $patchPath -Raw -Encoding UTF8

    # 分割补丁为单个文件的 diff
    $diffBlocks = @()
    $currentBlock = ""
    $currentFile = ""

    foreach ($line in $originalPatch -split "`n") {
        if ($line -match '^diff --git a/(.+) b/') {
            # 保存前一个块
            if ($currentBlock -and $currentFile) {
                $shouldInclude = $false
                foreach ($file in $filteredFiles) {
                    if ($currentFile -eq $file) {
                        $shouldInclude = $true
                        break
                    }
                }
                if ($shouldInclude) {
                    $diffBlocks += $currentBlock
                }
            }
            # 开始新块
            $currentFile = $matches[1]
            $currentBlock = $line + "`n"
        } else {
            $currentBlock += $line + "`n"
        }
    }

    # 处理最后一个块
    if ($currentBlock -and $currentFile) {
        $shouldInclude = $false
        foreach ($file in $filteredFiles) {
            if ($currentFile -eq $file) {
                $shouldInclude = $true
                break
            }
        }
        if ($shouldInclude) {
            $diffBlocks += $currentBlock
        }
    }

    # 合并所有块
    $patchContent = $diffBlocks -join ""

    # 写入补丁文件
    $patchContent | Out-File -FilePath $minimalPatchPath -Encoding utf8 -NoNewline -Force
    Push-Location $fclSourcePath

    # 显示统计信息
    $originalSize = if (Test-Path $patchPath) { (Get-Item $patchPath).Length } else { 0 }
    $newSize = (Get-Item $minimalPatchPath).Length
    $savings = if ($originalSize -gt 0) {
        [Math]::Round((1 - $newSize / $originalSize) * 100, 1)
    } else { 0 }

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "  ✓ 补丁生成成功！" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "统计信息：" -ForegroundColor Cyan
    Write-Host "  原补丁大小: $([Math]::Round($originalSize / 1MB, 2)) MB" -ForegroundColor Yellow
    Write-Host "  新补丁大小: $([Math]::Round($newSize / 1KB, 2)) KB" -ForegroundColor Green
    Write-Host "  节省空间:   $savings%" -ForegroundColor Green
    Write-Host "  输出路径:   $minimalPatchPath" -ForegroundColor Gray
    Write-Host ""
    Write-Host "下一步操作：" -ForegroundColor Cyan
    Write-Host "  1. 检查新补丁内容: git diff patches/fcl-kernel-mode-minimal.patch" -ForegroundColor Gray
    Write-Host "  2. 测试新补丁:     pwsh tools/scripts/test_minimal_patch.ps1" -ForegroundColor Gray
    Write-Host "  3. 替换旧补丁:     mv patches/fcl-kernel-mode-minimal.patch patches/fcl-kernel-mode.patch" -ForegroundColor Gray
    Write-Host ""
}
finally {
    Pop-Location
}
