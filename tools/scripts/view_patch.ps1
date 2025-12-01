<#
.SYNOPSIS
    查看 FCL 内核模式补丁的修改内容

.DESCRIPTION
    提供多种视图模式查看补丁文件的修改内容

.PARAMETER Mode
    查看模式：
    - summary: 文件列表和统计摘要（默认）
    - stats: 详细的修改统计
    - diff: 查看完整 diff（分页显示）
    - files: 仅列出文件名
    - category: 按类型分类显示

.PARAMETER File
    查看特定文件的修改

.PARAMETER UseMinimal
    使用最小化补丁（默认），否则使用原始补丁

.EXAMPLE
    .\view_patch.ps1
    # 显示摘要

.EXAMPLE
    .\view_patch.ps1 -Mode stats
    # 显示详细统计

.EXAMPLE
    .\view_patch.ps1 -File "include/fcl/logging.h"
    # 查看特定文件的修改

.EXAMPLE
    .\view_patch.ps1 -Mode category
    # 按文件类型分类显示
#>

param(
    [ValidateSet('summary', 'stats', 'diff', 'files', 'category')]
    [string]$Mode = 'summary',

    [string]$File = '',

    [switch]$UseMinimal = $true
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))

# 选择补丁文件
if ($UseMinimal) {
    $patchPath = Join-Path $repoRoot 'patches\fcl-kernel-mode-minimal.patch'
} else {
    $patchPath = Join-Path $repoRoot 'patches\fcl-kernel-mode.patch'
}

if (-not (Test-Path $patchPath)) {
    throw "未找到补丁文件: $patchPath"
}

# 解析补丁文件
function Parse-PatchFile {
    param([string]$Path)

    $content = Get-Content $Path -Raw -Encoding UTF8
    $files = @()
    $currentFile = $null

    foreach ($line in $content -split "`n") {
        if ($line -match '^diff --git a/(.+) b/(.+)$') {
            if ($currentFile) {
                $files += $currentFile
            }

            $currentFile = @{
                Path = $matches[1]
                OldPath = $matches[1]
                NewPath = $matches[2]
                Added = 0
                Deleted = 0
                IsNew = $false
                IsDeleted = $false
                DiffContent = @()
            }
        }
        elseif ($line -match '^new file mode') {
            $currentFile.IsNew = $true
        }
        elseif ($line -match '^deleted file mode') {
            $currentFile.IsDeleted = $true
        }
        elseif ($line -match '^\+(?!\+\+)') {
            $currentFile.Added++
        }
        elseif ($line -match '^-(?!--)') {
            $currentFile.Deleted++
        }

        if ($currentFile) {
            $currentFile.DiffContent += $line
        }
    }

    if ($currentFile) {
        $files += $currentFile
    }

    return $files
}

# 分类文件
function Group-FilesByCategory {
    param($Files)

    $categories = @{
        'Headers (include/)' = @()
        'Source (src/)' = @()
        'Tests' = @()
        'Build System' = @()
        'Documentation' = @()
        'Other' = @()
    }

    foreach ($file in $Files) {
        $path = $file.Path

        if ($path -match '^include/') {
            $categories['Headers (include/)'] += $file
        }
        elseif ($path -match '^src/') {
            $categories['Source (src/)'] += $file
        }
        elseif ($path -match '^test/') {
            $categories['Tests'] += $file
        }
        elseif ($path -match '\.(cmake|txt)$|^CMake') {
            $categories['Build System'] += $file
        }
        elseif ($path -match '\.(md|rst|txt)$|^doc/') {
            $categories['Documentation'] += $file
        }
        else {
            $categories['Other'] += $file
        }
    }

    return $categories
}

# 显示统计信息
function Show-PatchStats {
    param($Files, [string]$PatchName)

    $totalAdded = ($Files | Measure-Object -Property Added -Sum).Sum
    $totalDeleted = ($Files | Measure-Object -Property Deleted -Sum).Sum
    $newFiles = ($Files | Where-Object { $_.IsNew }).Count
    $deletedFiles = ($Files | Where-Object { $_.IsDeleted }).Count
    $modifiedFiles = $Files.Count - $newFiles - $deletedFiles

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  补丁统计: $PatchName" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "文件变更：" -ForegroundColor Yellow
    Write-Host "  总文件数:     $($Files.Count)" -ForegroundColor Gray
    Write-Host "  新增文件:     " -NoNewline -ForegroundColor Gray
    Write-Host "$newFiles" -ForegroundColor Green
    Write-Host "  修改文件:     " -NoNewline -ForegroundColor Gray
    Write-Host "$modifiedFiles" -ForegroundColor Yellow
    Write-Host "  删除文件:     " -NoNewline -ForegroundColor Gray
    Write-Host "$deletedFiles" -ForegroundColor Red
    Write-Host ""
    Write-Host "代码变更：" -ForegroundColor Yellow
    Write-Host "  新增行数:     " -NoNewline -ForegroundColor Gray
    Write-Host "+$totalAdded" -ForegroundColor Green
    Write-Host "  删除行数:     " -NoNewline -ForegroundColor Gray
    Write-Host "-$totalDeleted" -ForegroundColor Red
    Write-Host "  净变更:       " -NoNewline -ForegroundColor Gray
    $net = $totalAdded - $totalDeleted
    if ($net -gt 0) {
        Write-Host "+$net" -ForegroundColor Green
    } else {
        Write-Host "$net" -ForegroundColor Red
    }
    Write-Host ""
}

# 主逻辑
$patchName = Split-Path -Leaf $patchPath
Write-Host "正在解析补丁文件..." -ForegroundColor Yellow
$files = Parse-PatchFile -Path $patchPath

if ($File) {
    # 查看特定文件
    $targetFile = $files | Where-Object { $_.Path -eq $File -or $_.Path -like "*$File*" }

    if (-not $targetFile) {
        Write-Host "未找到文件: $File" -ForegroundColor Red
        Write-Host ""
        Write-Host "可用文件：" -ForegroundColor Yellow
        $files | ForEach-Object { Write-Host "  $($_.Path)" -ForegroundColor Gray }
        exit 1
    }

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  文件: $($targetFile.Path)" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""

    if ($targetFile.IsNew) {
        Write-Host "状态: " -NoNewline
        Write-Host "新增文件" -ForegroundColor Green
    } elseif ($targetFile.IsDeleted) {
        Write-Host "状态: " -NoNewline
        Write-Host "删除文件" -ForegroundColor Red
    } else {
        Write-Host "状态: " -NoNewline
        Write-Host "已修改" -ForegroundColor Yellow
    }

    Write-Host "变更: " -NoNewline
    Write-Host "+$($targetFile.Added) " -NoNewline -ForegroundColor Green
    Write-Host "-$($targetFile.Deleted)" -ForegroundColor Red
    Write-Host ""
    Write-Host "差异内容：" -ForegroundColor Yellow
    Write-Host "----------------------------------------" -ForegroundColor Gray

    foreach ($line in $targetFile.DiffContent) {
        if ($line -match '^\+') {
            Write-Host $line -ForegroundColor Green
        } elseif ($line -match '^-') {
            Write-Host $line -ForegroundColor Red
        } elseif ($line -match '^@@') {
            Write-Host $line -ForegroundColor Cyan
        } else {
            Write-Host $line -ForegroundColor Gray
        }
    }

    exit 0
}

switch ($Mode) {
    'summary' {
        Show-PatchStats -Files $files -PatchName $patchName

        Write-Host "最近修改的文件（前 10 个）：" -ForegroundColor Yellow
        $files | Sort-Object { $_.Added + $_.Deleted } -Descending | Select-Object -First 10 | ForEach-Object {
            $status = if ($_.IsNew) { "[新增]" } elseif ($_.IsDeleted) { "[删除]" } else { "[修改]" }
            $color = if ($_.IsNew) { "Green" } elseif ($_.IsDeleted) { "Red" } else { "Yellow" }

            Write-Host "  $status " -NoNewline -ForegroundColor $color
            Write-Host "$($_.Path) " -NoNewline -ForegroundColor Gray
            Write-Host "(+$($_.Added)/-$($_.Deleted))" -ForegroundColor DarkGray
        }
        Write-Host ""
    }

    'stats' {
        Show-PatchStats -Files $files -PatchName $patchName

        Write-Host "所有文件详细统计：" -ForegroundColor Yellow
        Write-Host ""

        $files | Sort-Object Path | ForEach-Object {
            $status = if ($_.IsNew) { "新增" } elseif ($_.IsDeleted) { "删除" } else { "修改" }
            $color = if ($_.IsNew) { "Green" } elseif ($_.IsDeleted) { "Red" } else { "Yellow" }

            Write-Host "  [$status] " -NoNewline -ForegroundColor $color
            Write-Host "$($_.Path)" -ForegroundColor Gray
            Write-Host "         +$($_.Added) -$($_.Deleted)" -ForegroundColor DarkGray
        }
        Write-Host ""
    }

    'files' {
        Write-Host ""
        Write-Host "补丁包含的文件列表 ($($files.Count) 个)：" -ForegroundColor Cyan
        Write-Host ""

        $files | Sort-Object Path | ForEach-Object {
            if ($_.IsNew) {
                Write-Host "  [+] $($_.Path)" -ForegroundColor Green
            } elseif ($_.IsDeleted) {
                Write-Host "  [-] $($_.Path)" -ForegroundColor Red
            } else {
                Write-Host "  [M] $($_.Path)" -ForegroundColor Yellow
            }
        }
        Write-Host ""
    }

    'category' {
        Show-PatchStats -Files $files -PatchName $patchName

        $categories = Group-FilesByCategory -Files $files

        Write-Host "按类型分组：" -ForegroundColor Yellow
        Write-Host ""

        foreach ($category in $categories.Keys | Sort-Object) {
            $items = $categories[$category]
            if ($items.Count -gt 0) {
                Write-Host "  $category ($($items.Count) 个文件):" -ForegroundColor Cyan

                $items | Sort-Object Path | ForEach-Object {
                    $status = if ($_.IsNew) { "[+]" } elseif ($_.IsDeleted) { "[-]" } else { "[M]" }
                    $color = if ($_.IsNew) { "Green" } elseif ($_.IsDeleted) { "Red" } else { "Yellow" }

                    Write-Host "    $status " -NoNewline -ForegroundColor $color
                    Write-Host "$($_.Path) " -NoNewline -ForegroundColor Gray
                    Write-Host "(+$($_.Added)/-$($_.Deleted))" -ForegroundColor DarkGray
                }
                Write-Host ""
            }
        }
    }

    'diff' {
        Write-Host ""
        Write-Host "补丁完整内容（分页显示）：" -ForegroundColor Cyan
        Write-Host "提示：使用空格翻页，Q 退出" -ForegroundColor Gray
        Write-Host ""
        Start-Sleep -Seconds 1

        Get-Content $patchPath | Out-Host -Paging
    }
}
