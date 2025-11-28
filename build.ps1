<#
.SYNOPSIS
    FCL+Musa 一站式构建入口，调用 scripts/build_interactive.ps1。
#>

param(
    [string[]]$Args
)

$scriptPath = Join-Path $PSScriptRoot "scripts/build_interactive.ps1"
if (-not (Test-Path $scriptPath)) {
    Write-Error "找不到脚本 $scriptPath"
    exit 1
}

& powershell -ExecutionPolicy Bypass -File $scriptPath @Args
