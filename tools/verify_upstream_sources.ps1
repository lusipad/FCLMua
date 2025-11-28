param(
    [string]$WorkDir = "$env:TEMP\\fclmusa-upstream-check",
    [string]$FclCommit = "5f7776e2101b8ec95d5054d732684d00dac45e3d",
    [string]$LibccdRepo = "https://github.com/danfis/libccd",
    [string]$LibccdRef = "",
    [string]$EigenRepo = "https://gitlab.com/libeigen/eigen.git",
    [string]$EigenRef = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Step($message, [ScriptBlock]$action) {
    Write-Host "[*] $message"
    & $action
}

function Ensure-Dir($path) {
    if (-not (Test-Path $path)) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
    }
}

$root = Resolve-Path "."
$work = $WorkDir
Ensure-Dir $work

Invoke-Step "Cloning upstream FCL to $work\\fcl" {
    if (-not (Test-Path "$work\\fcl\\.git")) {
        git clone https://github.com/flexible-collision-library/fcl $work\\fcl
    }
    git -C $work\\fcl fetch --all --prune
    git -C $work\\fcl checkout $FclCommit
}

if ($LibccdRef) {
    Invoke-Step "Cloning upstream libccd to $work\\libccd" {
        if (-not (Test-Path "$work\\libccd\\.git")) {
            git clone $LibccdRepo $work\\libccd
        }
        git -C $work\\libccd fetch --all --prune
        git -C $work\\libccd checkout $LibccdRef
    }
}

if ($EigenRef) {
    Invoke-Step "Cloning upstream Eigen to $work\\eigen" {
        if (-not (Test-Path "$work\\eigen\\.git")) {
            git clone $EigenRepo $work\\eigen
        }
        git -C $work\\eigen fetch --all --prune
        git -C $work\\eigen checkout $EigenRef
    }
}

Write-Host "`n== Diff: FCL (repo fcl-source vs upstream)" -ForegroundColor Cyan
git diff --no-index --stat --ignore-cr-at-eol "$root\\fcl-source" "$work\\fcl"

if ($LibccdRef) {
    Write-Host "`n== Diff: libccd (external/libccd vs upstream)" -ForegroundColor Cyan
    git diff --no-index --stat --ignore-cr-at-eol "$root\\external\\libccd" "$work\\libccd"
}

if ($EigenRef) {
    Write-Host "`n== Diff: Eigen (external/Eigen vs upstream)" -ForegroundColor Cyan
    git diff --no-index --stat --ignore-cr-at-eol "$root\\external\\Eigen" "$work\\eigen"
}

Write-Host "`nDone. Detailed diffs: run git diff --no-index <local> <upstream> without --stat." -ForegroundColor Green
