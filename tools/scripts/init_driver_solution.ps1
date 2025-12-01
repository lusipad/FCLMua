<#
.SYNOPSIS
    初始化 kernel/driver/msbuild 的 VS 解决方案模板。

.DESCRIPTION
    从 tools/templates/driver 复制标准化的 .sln/.vcxproj/.filters，
    方便所有开发者快速获得与构建脚本一致的工程文件。

.PARAMETER Force
    覆盖已存在的目标文件。默认存在则跳过。
#>

[CmdletBinding()]
param(
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $toolsDir
$templateDir = Join-Path $repoRoot 'tools\templates\driver'
$targetDir = Join-Path $repoRoot 'kernel\driver\msbuild'

if (-not (Test-Path -Path $templateDir -PathType Container)) {
    throw "模板目录不存在：$templateDir"
}

if (-not (Test-Path -Path $targetDir -PathType Container)) {
    New-Item -ItemType Directory -Path $targetDir | Out-Null
}

$templateFiles = Get-ChildItem -Path $templateDir -File
if ($templateFiles.Count -eq 0) {
    throw "模板目录为空：$templateDir"
}

Write-Host "正在生成 VS 工程文件..." -ForegroundColor Cyan
$generated = @()

foreach ($file in $templateFiles) {
    $destination = Join-Path $targetDir $file.Name
    if ((Test-Path $destination) -and -not $Force) {
        Write-Host "  跳过（已存在）：$($file.Name)" -ForegroundColor Yellow
        continue
    }

    Copy-Item -Path $file.FullName -Destination $destination -Force
    $generated += $file.Name
    Write-Host "  写入：$($file.Name)" -ForegroundColor Green
}

if ($generated.Count -eq 0) {
    Write-Host "没有文件被覆盖。若需强制刷新，请添加 -Force。" -ForegroundColor Yellow
}
else {
    Write-Host ""
    Write-Host "生成完成，工程路径：" -ForegroundColor Green
    Write-Host "  $targetDir" -ForegroundColor Gray
}
