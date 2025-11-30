<#
.SYNOPSIS
    解析 WDK 环境并输出批处理变量设置命令

.DESCRIPTION
    调用 common.psm1 中的 Resolve-WdkEnvironment 函数，
    将结果转换为 batch 环境变量设置命令。

.PARAMETER EmitBatch
    输出 batch 格式的环境变量设置命令
#>

param(
    [switch]$EmitBatch
)

$ErrorActionPreference = 'Stop'

# 导入公共函数库
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
if (-not $scriptDir) {
    $scriptDir = $PSScriptRoot
}
$commonPath = Join-Path $scriptDir 'common.psm1'

if (-not (Test-Path $commonPath)) {
    [Console]::Error.WriteLine("ERROR: common.psm1 not found at: $commonPath")
    exit 1
}

Import-Module $commonPath -Force -WarningAction SilentlyContinue

# 检查环境变量或使用自动探测
$requestedVersion = $env:FCL_MUSA_WDK_VERSION

try {
    $wdkParams = @{}
    if ($requestedVersion) {
        $wdkParams['RequestedVersion'] = $requestedVersion
    }

    $wdkInfo = Resolve-WdkEnvironment @wdkParams

    if ($EmitBatch) {
        # 输出批处理变量设置命令
        if ($wdkInfo.Root) {
            Write-Output "set `"WDK_ROOT=$($wdkInfo.Root)`""
        }
        Write-Output "set `"WDK_VERSION=$($wdkInfo.Version)`""

        if ($wdkInfo.IncludePaths -and $wdkInfo.IncludePaths.Count -gt 0) {
            $includeStr = $wdkInfo.IncludePaths -join ';'
            Write-Output "set `"WDK_RESOLVED_INCLUDE=$includeStr`""
        }

        if ($wdkInfo.LibPaths -and $wdkInfo.LibPaths.Count -gt 0) {
            $libStr = $wdkInfo.LibPaths -join ';'
            Write-Output "set `"WDK_RESOLVED_LIB=$libStr`""
        }

        if ($wdkInfo.BinPaths -and $wdkInfo.BinPaths.Count -gt 0) {
            $binStr = $wdkInfo.BinPaths -join ';'
            Write-Output "set `"WDK_RESOLVED_BIN=$binStr`""
        }
    }
    else {
        # 人类可读格式
        Write-Host "WDK Environment:" -ForegroundColor Cyan
        Write-Host "  Root:    $($wdkInfo.Root)"
        Write-Host "  Version: $($wdkInfo.Version)"
        Write-Host "  Include: $($wdkInfo.IncludePaths.Count) paths"
        Write-Host "  Lib:     $($wdkInfo.LibPaths.Count) paths"
        Write-Host "  Bin:     $($wdkInfo.BinPaths.Count) paths"
    }

    exit 0
}
catch {
    [Console]::Error.WriteLine("ERROR: Failed to resolve WDK environment: $_")
    exit 1
}
