<#
.SYNOPSIS
    下载并静默安装指定版本的 Windows Driver Kit (WDK)。

.DESCRIPTION
    该脚本会将 wdksetup.exe 下载到临时目录，并以 /quiet /norestart 模式
    安装完整的 WDK 功能集。安装完成后，会自动设置 WDKContentRoot 与
    FCL_MUSA_WDK_VERSION（含 GitHub Actions 的 GITHUB_ENV 输出）。

.PARAMETER Version
    目标 WDK 版本号（例如 10.0.22621.0）。

.PARAMETER DownloadUrl
    官方 wdksetup.exe 的下载地址。可使用 Microsoft Learn 文档中的 go.microsoft 链接。

.PARAMETER InstallDir
    可选，WDK 安装目录。默认是 C:\Program Files (x86)\Windows Kits\10。
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$DownloadUrl,

    [string]$InstallDir = "${env:ProgramFiles(x86)}\Windows Kits\10"
)

$ErrorActionPreference = 'Stop'

if (-not $Version.Trim()) {
    throw "Version 不能为空。"
}
if (-not $DownloadUrl.Trim()) {
    throw "DownloadUrl 不能为空。"
}

Write-Host "Installing WDK $Version ..." -ForegroundColor Cyan

$sanitizedVersion = ($Version -replace '[^0-9]', '_')
$installerPath = Join-Path ([System.IO.Path]::GetTempPath()) "wdksetup_$sanitizedVersion.exe"

Write-Host "  Downloading WDK installer..." -ForegroundColor Yellow
Invoke-WebRequest -Uri $DownloadUrl -OutFile $installerPath -UseBasicParsing

if (-not (Test-Path $installerPath -PathType Leaf)) {
    throw "下载失败：$installerPath"
}

$arguments = @(
    '/features', 'OptionId.WDKComplete',
    '/quiet',
    '/norestart',
    '/ceip', 'off',
    '/InstallPath', "`"$InstallDir`""
)

Write-Host "  Running wdksetup.exe (静默安装)..." -ForegroundColor Yellow
& $installerPath @arguments
if ($LASTEXITCODE -ne 0) {
    throw "WDK 安装失败，退出码 $LASTEXITCODE"
}

$expectedInclude = Join-Path $InstallDir "Include\$Version"
if (-not (Test-Path $expectedInclude)) {
    Write-Warning "未检测到 $expectedInclude，默认使用 $InstallDir\Include 目录中最新版本。"
    $latest = Get-ChildItem -Path (Join-Path $InstallDir 'Include') -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if ($latest) {
        $Version = $latest.Name
        $expectedInclude = $latest.FullName
    }
}

if (-not (Test-Path $expectedInclude)) {
    throw "WDK 安装完成，但未找到任何 Include 版本目录：$InstallDir\Include"
}

$env:WDKContentRoot = $InstallDir
$env:FCL_MUSA_WDK_VERSION = $Version

if ($env:GITHUB_ENV) {
    Add-Content -Path $env:GITHUB_ENV -Value "WDKContentRoot=$InstallDir"
    Add-Content -Path $env:GITHUB_ENV -Value "FCL_MUSA_WDK_VERSION=$Version"
}

Write-Host "WDK $Version installed at $InstallDir" -ForegroundColor Green
