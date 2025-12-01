<#
.SYNOPSIS
    对比 FCL 补丁应用前后的差异

.DESCRIPTION
    提供多种 diff 对比功能：
    - 对比补丁应用前后的文件差异
    - 对比两个补丁文件的差异
    - 对比特定文件的修改

.PARAMETER Mode
    对比模式：
    - before-after: 对比应用补丁前后的差异（默认）
    - patches: 对比两个补丁文件（原始 vs 最小化）
    - file: 对比特定文件的修改
    - side-by-side: 并排显示差异

.PARAMETER File
    指定要对比的文件路径

.PARAMETER Tool
    使用的 diff 工具：
    - git: 使用 git diff（默认，彩色输出）
    - vscode: 在 VS Code 中打开对比
    - beyond: 使用 Beyond Compare
    - meld: 使用 Meld

.EXAMPLE
    .\diff_patch.ps1
    # 对比补丁应用前后

.EXAMPLE
    .\diff_patch.ps1 -File "include/fcl/logging.h"
    # 对比特定文件

.EXAMPLE
    .\diff_patch.ps1 -Mode patches
    # 对比两个补丁文件

.EXAMPLE
    .\diff_patch.ps1 -Tool vscode
    # 在 VS Code 中查看对比
#>

param(
    [ValidateSet('before-after', 'patches', 'file', 'side-by-side')]
    [string]$Mode = 'before-after',

    [string]$File = '',

    [ValidateSet('git', 'vscode', 'beyond', 'meld')]
    [string]$Tool = 'git'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$fclSourcePath = Join-Path $repoRoot 'external\fcl-source'
$minimalPatchPath = Join-Path $repoRoot 'patches\fcl-kernel-mode-minimal.patch'
$fullPatchPath = Join-Path $repoRoot 'patches\fcl-kernel-mode.patch'

# 检查 Git 是否可用
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "未找到 git 命令，请先安装 Git"
}

# 检查 fcl-source 是否是 Git 仓库
if (-not (Test-Path (Join-Path $fclSourcePath '.git'))) {
    throw "external/fcl-source 不是有效的 Git 子模块"
}

function Show-GitDiff {
    param(
        [string]$Path,
        [string]$FilePath = '',
        [switch]$Cached,
        [switch]$NameOnly
    )

    Push-Location $Path
    try {
        $args = @('diff')

        if ($Cached) {
            $args += '--cached'
        }

        if ($NameOnly) {
            $args += '--name-only'
        } else {
            $args += @('--color=always', '--stat', '--patch')
        }

        if ($FilePath) {
            $args += '--'
            $args += $FilePath
        }

        & git @args
    }
    finally {
        Pop-Location
    }
}

function Open-InVSCode {
    param(
        [string]$LeftPath,
        [string]$RightPath
    )

    if (-not (Get-Command code -ErrorAction SilentlyContinue)) {
        throw "未找到 VS Code (code 命令)，请确保已安装并加入 PATH"
    }

    & code --diff $LeftPath $RightPath
}

function Show-BeforeAfterDiff {
    param([string]$FilePath = '')

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  对比：应用补丁前 vs 应用补丁后" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""

    # 检查当前状态
    Push-Location $fclSourcePath
    try {
        $status = & git status --short
        $hasPatch = $status.Count -gt 0

        if (-not $hasPatch) {
            Write-Host "⚠️  当前 fcl-source 是干净状态（未应用补丁）" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "是否要应用补丁后查看差异？(y/N): " -NoNewline -ForegroundColor Yellow
            $response = Read-Host

            if ($response -eq 'y' -or $response -eq 'Y') {
                Pop-Location
                Write-Host "正在应用补丁..." -ForegroundColor Yellow
                & (Join-Path $repoRoot 'tools\scripts\apply_fcl_patch.ps1') -Quiet
                Push-Location $fclSourcePath
            } else {
                Write-Host "操作已取消" -ForegroundColor Gray
                return
            }
        }

        Write-Host "显示修改内容：" -ForegroundColor Yellow
        Write-Host ""

        if ($FilePath) {
            # 对比特定文件
            Write-Host "文件: $FilePath" -ForegroundColor Cyan
            Write-Host "----------------------------------------" -ForegroundColor Gray
            & git diff HEAD -- $FilePath
        } else {
            # 显示所有修改
            Write-Host "统计信息：" -ForegroundColor Yellow
            & git diff HEAD --stat
            Write-Host ""
            Write-Host "详细差异（按 q 退出）：" -ForegroundColor Yellow
            Write-Host ""
            Start-Sleep -Milliseconds 500
            & git diff HEAD --color=always | Out-Host -Paging
        }
    }
    finally {
        Pop-Location
    }
}

function Compare-PatchFiles {
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  对比：原始补丁 vs 最小化补丁" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""

    if (-not (Test-Path $fullPatchPath)) {
        throw "未找到原始补丁: $fullPatchPath"
    }

    if (-not (Test-Path $minimalPatchPath)) {
        throw "未找到最小化补丁: $minimalPatchPath"
    }

    # 显示基本信息
    $fullSize = (Get-Item $fullPatchPath).Length
    $minimalSize = (Get-Item $minimalPatchPath).Length
    $savings = [Math]::Round((1 - $minimalSize / $fullSize) * 100, 1)

    Write-Host "文件大小对比：" -ForegroundColor Yellow
    Write-Host "  原始补丁:   $([Math]::Round($fullSize / 1MB, 2)) MB" -ForegroundColor Red
    Write-Host "  最小化补丁: $([Math]::Round($minimalSize / 1KB, 2)) KB" -ForegroundColor Green
    Write-Host "  节省空间:   $savings%" -ForegroundColor Green
    Write-Host ""

    # 对比文件数量
    Write-Host "文件数量对比：" -ForegroundColor Yellow

    $fullFiles = (Select-String -Path $fullPatchPath -Pattern '^diff --git').Count
    $minimalFiles = (Select-String -Path $minimalPatchPath -Pattern '^diff --git').Count

    Write-Host "  原始补丁:   $fullFiles 个文件" -ForegroundColor Red
    Write-Host "  最小化补丁: $minimalFiles 个文件" -ForegroundColor Green
    Write-Host "  减少:       $($fullFiles - $minimalFiles) 个文件" -ForegroundColor Green
    Write-Host ""

    # 列出被排除的文件
    Write-Host "最小化补丁排除的文件类型：" -ForegroundColor Yellow

    $fullFilesList = Select-String -Path $fullPatchPath -Pattern '^diff --git a/(.+) b/' |
        ForEach-Object { $_.Matches.Groups[1].Value }

    $minimalFilesList = Select-String -Path $minimalPatchPath -Pattern '^diff --git a/(.+) b/' |
        ForEach-Object { $_.Matches.Groups[1].Value }

    $excluded = $fullFilesList | Where-Object { $_ -notin $minimalFilesList }

    $excludedByType = @{
        'CI/CD' = @()
        'Build System' = @()
        'Tests' = @()
        'Documentation' = @()
        'Other' = @()
    }

    foreach ($file in $excluded) {
        if ($file -match '\.(yml|yaml|travis)$|^\.github/') {
            $excludedByType['CI/CD'] += $file
        }
        elseif ($file -match 'CMakeLists\.txt|\.cmake$|^CMakeModules/') {
            $excludedByType['Build System'] += $file
        }
        elseif ($file -match '^test/') {
            $excludedByType['Tests'] += $file
        }
        elseif ($file -match '\.(md|rst|txt)$') {
            $excludedByType['Documentation'] += $file
        }
        else {
            $excludedByType['Other'] += $file
        }
    }

    foreach ($type in $excludedByType.Keys | Sort-Object) {
        $files = $excludedByType[$type]
        if ($files.Count -gt 0) {
            Write-Host "  $type ($($files.Count) 个):" -ForegroundColor Cyan
            $files | Select-Object -First 5 | ForEach-Object {
                Write-Host "    - $_" -ForegroundColor Gray
            }
            if ($files.Count -gt 5) {
                Write-Host "    ... 还有 $($files.Count - 5) 个文件" -ForegroundColor DarkGray
            }
        }
    }
    Write-Host ""
}

function Show-FileDiff {
    param([string]$FilePath)

    if (-not $FilePath) {
        throw "请指定要对比的文件路径，使用 -File 参数"
    }

    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  文件对比: $FilePath" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""

    Push-Location $fclSourcePath
    try {
        # 检查文件是否存在于补丁中
        $patchContent = Get-Content $minimalPatchPath -Raw
        $filePattern = [regex]::Escape($FilePath)

        if ($patchContent -notmatch "diff --git.*$filePattern") {
            Write-Host "⚠️  文件 '$FilePath' 不在补丁中" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "提示：使用以下命令查看补丁中的文件：" -ForegroundColor Gray
            Write-Host "  pwsh tools/scripts/view_patch.ps1 -Mode files" -ForegroundColor Gray
            return
        }

        # 检查当前状态
        $status = & git status --short $FilePath 2>$null

        if (-not $status) {
            Write-Host "⚠️  文件未修改，正在应用补丁..." -ForegroundColor Yellow
            Pop-Location
            & (Join-Path $repoRoot 'tools\scripts\apply_fcl_patch.ps1') -Quiet
            Push-Location $fclSourcePath
        }

        # 显示文件差异
        Write-Host "差异内容：" -ForegroundColor Yellow
        Write-Host ""

        if ($Tool -eq 'vscode') {
            # 创建临时文件存储原始版本
            $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "fcl-diff"
            if (-not (Test-Path $tempDir)) {
                New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
            }

            $tempFile = Join-Path $tempDir (Split-Path -Leaf $FilePath)
            & git show "HEAD:$FilePath" > $tempFile 2>$null

            if (Test-Path $tempFile) {
                Write-Host "在 VS Code 中打开对比..." -ForegroundColor Green
                $currentFile = Join-Path $fclSourcePath $FilePath
                Open-InVSCode -LeftPath $tempFile -RightPath $currentFile
            } else {
                Write-Host "⚠️  文件可能是新增的，使用 git diff 查看" -ForegroundColor Yellow
                & git diff HEAD -- $FilePath
            }
        }
        else {
            & git diff HEAD --color=always -- $FilePath | Out-Host -Paging
        }
    }
    finally {
        Pop-Location
    }
}

function Show-SideBySide {
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  并排对比查看" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""

    Push-Location $fclSourcePath
    try {
        $modifiedFiles = & git diff HEAD --name-only | Where-Object { $_ -match '^(include|src)/' }

        if ($modifiedFiles.Count -eq 0) {
            Write-Host "⚠️  没有修改的文件" -ForegroundColor Yellow
            return
        }

        Write-Host "修改的文件 ($($modifiedFiles.Count) 个)：" -ForegroundColor Yellow
        for ($i = 0; $i -lt $modifiedFiles.Count; $i++) {
            Write-Host "  [$($i + 1)] $($modifiedFiles[$i])" -ForegroundColor Gray
        }
        Write-Host ""
        Write-Host "选择要查看的文件编号 (1-$($modifiedFiles.Count))，或按 Enter 查看全部: " -NoNewline
        $choice = Read-Host

        if ($choice) {
            $index = [int]$choice - 1
            if ($index -ge 0 -and $index -lt $modifiedFiles.Count) {
                $selectedFile = $modifiedFiles[$index]

                if ($Tool -eq 'vscode') {
                    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "fcl-diff"
                    if (-not (Test-Path $tempDir)) {
                        New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
                    }

                    $tempFile = Join-Path $tempDir (Split-Path -Leaf $selectedFile)
                    & git show "HEAD:$selectedFile" > $tempFile 2>$null

                    Write-Host "在 VS Code 中打开对比..." -ForegroundColor Green
                    $currentFile = Join-Path $fclSourcePath $selectedFile
                    Open-InVSCode -LeftPath $tempFile -RightPath $currentFile
                }
                else {
                    & git diff HEAD --color=always -- $selectedFile | Out-Host -Paging
                }
            }
        }
        else {
            Write-Host ""
            Write-Host "显示所有文件的差异：" -ForegroundColor Yellow
            & git diff HEAD --color=always | Out-Host -Paging
        }
    }
    finally {
        Pop-Location
    }
}

# 主逻辑
switch ($Mode) {
    'before-after' {
        Show-BeforeAfterDiff -FilePath $File
    }

    'patches' {
        Compare-PatchFiles
    }

    'file' {
        Show-FileDiff -FilePath $File
    }

    'side-by-side' {
        Show-SideBySide
    }
}
