<#
.SYNOPSIS
    一站式交互式构建脚本，支持选择用户态/内核态构建并自动探测 WDK。

.NOTES
    - 建议在 VS Developer Command Prompt 中运行，确保 cl/nmake/cmake 可用。
    - 默认使用 NMake Makefiles 生成器；若需切换，可手动调整 $generator。
#>

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Read-Choice {
    param(
        [string]$Prompt,
        [string[]]$Options,
        [string]$Default = $null
    )
    while ($true) {
        $choices = $Options -join '/'
        $input = Read-Host "$Prompt [$choices]$([string]::IsNullOrEmpty($Default) ? '' : " (default: $Default)")"
        if ([string]::IsNullOrEmpty($input) -and $Default) {
            return $Default
        }
        foreach ($opt in $Options) {
            if ($input -eq $opt) { return $opt }
        }
        Write-Warning "无效输入，请重试。"
    }
}

function Get-AvailableWdkVersions {
    param([string]$Root)
    if (-not (Test-Path $Root)) { return @() }
    Get-ChildItem -Path (Join-Path $Root 'Include') -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName 'km\ntddk.h') } |
        Select-Object -ExpandProperty Name |
        Sort-Object { $_ } -Descending
}

function Resolve-Wdk {
    param(
        [string]$PreferredRoot,
        [string]$PreferredVersion
    )
    $root = $PreferredRoot
    if (-not $root) {
        if ($env:FCL_MUSA_WDK_ROOT) { $root = $env:FCL_MUSA_WDK_ROOT }
        elseif ($env:WDKContentRoot) { $root = $env:WDKContentRoot }
        else { $root = "${env:ProgramFiles(x86)}\Windows Kits\10" }
    }

    if (-not (Test-Path $root)) {
        return @{ Ok = $false; Error = "未找到 WDK 根目录：$root" }
    }

    $versions = Get-AvailableWdkVersions -Root $root
    if (-not $versions -or $versions.Count -eq 0) {
        return @{ Ok = $false; Error = "未在 $root/Include 下找到任何 WDK 版本（缺少 km/ntddk.h）。" }
    }

    $version = $PreferredVersion
    if (-not $version) {
        if ($env:FCL_MUSA_WDK_VERSION) { $version = $env:FCL_MUSA_WDK_VERSION }
        else { $version = $versions[0] }
    }

    if (-not (Test-Path (Join-Path $root "Include/$version/km/ntddk.h"))) {
        return @{ Ok = $false; Error = "指定的 WDK 版本不存在：$version，可用版本：$($versions -join ', ')" }
    }

    return @{ Ok = $true; Root = $root; Version = $version }
}

function Run-CMake {
    param(
        [string]$SourceDir,
        [string]$BuildDir,
        [hashtable]$Options,
        [string]$Generator = 'NMake Makefiles'
    )
    $cmakeArgs = @("-S", $SourceDir, "-B", $BuildDir, "-G", $Generator)
    foreach ($k in $Options.Keys) {
        $cmakeArgs += "-D$k=$($Options[$k])"
    }
    Write-Host "CMake configure: $($cmakeArgs -join ' ')" -ForegroundColor Cyan
    cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }

    Write-Host "CMake build..." -ForegroundColor Cyan
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "CMake 构建失败" }
}

function Start-BuildFlow {
    $root = (Resolve-Path '..').ProviderPath
    $config = Read-Choice -Prompt "选择构建配置" -Options @('Debug','Release') -Default 'Debug'
    $target = Read-Choice -Prompt "选择构建类型" -Options @('user','kernel','both') -Default 'user'
    $buildDir = Read-Host "输入构建输出目录 (默认: build/interactive_$target)" 
    if ([string]::IsNullOrEmpty($buildDir)) { $buildDir = "build/interactive_$target" }

    $cmakeOptions = @{
        FCLMUSA_BUILD_USERLIB   = ($target -eq 'user' -or $target -eq 'both') ? 'ON' : 'OFF'
        FCLMUSA_BUILD_KERNEL_LIB = ($target -eq 'kernel' -or $target -eq 'both') ? 'ON' : 'OFF'
        FCLMUSA_BUILD_DRIVER    = 'OFF'
    }

    # Kernel 构建需要 WDK
    if ($cmakeOptions.FCLMUSA_BUILD_KERNEL_LIB -eq 'ON') {
        $wdk = Resolve-Wdk -PreferredRoot $null -PreferredVersion $null
        if (-not $wdk.Ok) { throw $wdk.Error }
        Write-Host "使用 WDK Root: $($wdk.Root) Version: $($wdk.Version)" -ForegroundColor Green
        $cmakeOptions.FCLMUSA_WDK_ROOT = $wdk.Root
        $cmakeOptions.FCLMUSA_WDK_VERSION = $wdk.Version
    }

    # Generator 强制使用单配置生成器，配置通过 CMake 变量传递
    Run-CMake -SourceDir $root -BuildDir $buildDir -Options $cmakeOptions

    Write-Host ""
    Write-Host "完成。产物路径示例：" -ForegroundColor Green
    if ($cmakeOptions.FCLMUSA_BUILD_USERLIB -eq 'ON') {
        Write-Host "  $buildDir/FclMusaCoreUser.lib" -ForegroundColor Gray
    }
    if ($cmakeOptions.FCLMUSA_BUILD_KERNEL_LIB -eq 'ON') {
        Write-Host "  $buildDir/FclMusaCore.lib" -ForegroundColor Gray
    }
    Write-Host ""
}

try {
    Start-BuildFlow
} catch {
    Write-Error $_
    exit 1
}
