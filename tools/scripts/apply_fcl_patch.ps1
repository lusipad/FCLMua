param(
    [switch]$Quiet,
    [switch]$Restore
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (
    Split-Path -Parent (
        Split-Path -Parent $MyInvocation.MyCommand.Path))
$submodulePath = Join-Path $repoRoot 'external/fcl-source'
$patchPath = Join-Path $repoRoot 'patches/fcl-kernel-mode.patch'

if (-not (Test-Path $patchPath)) {
    throw "未找到补丁文件: $patchPath"
}

if (-not (Test-Path $submodulePath)) {
    throw "未找到 FCL 源码目录: $submodulePath"
}

$submoduleGitPath = Join-Path $submodulePath '.git'
if (-not (Test-Path $submoduleGitPath)) {
    throw "external/fcl-source 不是有效的 Git 子模块，请先执行 `git submodule update --init --recursive`."
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "未找到 git 命令，请先安装 Git 并加入 PATH。"
}

function Invoke-Git {
    param([string[]]$Arguments)
    $output = & git -C $submodulePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -and -not $Quiet) {
        Write-Warning "git $($Arguments -join ' ') exited with code $exitCode"
        if ($output) { Write-Warning ($output -join "`n") }
    }
    return $exitCode
}

if ($Restore) {
    if ((Invoke-Git -Arguments @('reset', '--hard')) -ne 0) {
        throw "git reset --hard 执行失败，请检查 external/fcl-source 状态。"
    }
    if ((Invoke-Git -Arguments @('clean', '-fd')) -ne 0) {
        throw "git clean -fd 执行失败，请检查 external/fcl-source 权限。"
    }
    if (-not $Quiet) {
        Write-Host "已将 external/fcl-source 恢复为上游提交（删除所有本地改动）。" -ForegroundColor Cyan
    }
    return
}

$needsApply = $false
$checkResult = Invoke-Git -Arguments @('apply', '--check', $patchPath)
if ($checkResult -eq 0) {
    $needsApply = $true
} elseif ((Invoke-Git -Arguments @('apply', '--check', '--reverse', $patchPath)) -eq 0) {
    $needsApply = $false
} else {
    if (-not $Quiet) {
        Write-Host "检查补丁状态失败，尝试先重置 fcl-source..." -ForegroundColor Yellow
    }
    if ((Invoke-Git -Arguments @('reset', '--hard')) -eq 0) {
        $checkResult = Invoke-Git -Arguments @('apply', '--check', $patchPath)
        if ($checkResult -eq 0) {
            $needsApply = $true
        } else {
            throw "无法应用补丁，即使在重置后。补丁文件可能不匹配当前的 FCL 版本。"
        }
    } else {
        throw "无法应用补丁，请先运行 `git -C external/fcl-source checkout -- .` 或重新同步子模块后重试。"
    }
}

if ($needsApply) {
    if ((Invoke-Git -Arguments @('apply', $patchPath)) -ne 0) {
        throw "应用补丁失败。"
    }
    if (-not $Quiet) {
        Write-Host "已应用 FCL Kernel 模式补丁。" -ForegroundColor Green
    }
} elseif (-not $Quiet) {
    Write-Host "FCL Kernel 模式补丁已应用，无需重复操作。" -ForegroundColor Yellow
}
