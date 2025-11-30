<#
.SYNOPSIS
    环境检查脚本

.DESCRIPTION
    检查构建环境是否正确配置
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$script:ToolsDir = Join-Path $script:RepoRoot 'tools'

function Write-TaskHeader {
    param([string]$Title)
    Write-Host "`n============================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "============================================`n" -ForegroundColor Cyan
}

function Test-Command {
    param([string]$Name, [string]$Command)
    
    $result = Get-Command $Command -ErrorAction SilentlyContinue
    
    if ($result) {
        Write-Host "  ✓ $Name" -ForegroundColor Green -NoNewline
        Write-Host " - $($result.Source)" -ForegroundColor Gray
        return $true
    } else {
        Write-Host "  ✗ $Name" -ForegroundColor Red -NoNewline
        Write-Host " - 未找到" -ForegroundColor Gray
        return $false
    }
}

Write-TaskHeader "检查构建环境"

$allOk = $true

# 检查基础工具
Write-Host "基础工具:" -ForegroundColor Yellow
$allOk = (Test-Command "Git" "git") -and $allOk
$allOk = (Test-Command "CMake" "cmake") -and $allOk
$allOk = (Test-Command "PowerShell" "powershell") -and $allOk

# 检查编译器
Write-Host "`n编译器:" -ForegroundColor Yellow
$allOk = (Test-Command "MSBuild" "msbuild") -and $allOk
$allOk = (Test-Command "CL (MSVC)" "cl") -and $allOk

# 检查 WDK
Write-Host "`nWindows Driver Kit:" -ForegroundColor Yellow

$wdkPaths = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\inf2cat.exe",
    "C:\Program Files (x86)\Windows Kits\10\Include\*\km\wdm.h"
)

$wdkFound = $false
foreach ($pattern in $wdkPaths) {
    $found = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $wdkFound = $true
        Write-Host "  ✓ WDK" -ForegroundColor Green -NoNewline
        Write-Host " - $($found.DirectoryName)" -ForegroundColor Gray
        break
    }
}

if (-not $wdkFound) {
    Write-Host "  ✗ WDK" -ForegroundColor Red -NoNewline
    Write-Host " - 未找到" -ForegroundColor Gray
    $allOk = $false
}

# 检查 Visual Studio
Write-Host "`nVisual Studio:" -ForegroundColor Yellow
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (Test-Path $vsWhere) {
    $vsInfo = & $vsWhere -latest -format json | ConvertFrom-Json
    if ($vsInfo) {
        Write-Host "  ✓ Visual Studio $($vsInfo.installationVersion)" -ForegroundColor Green -NoNewline
        Write-Host " - $($vsInfo.installationPath)" -ForegroundColor Gray
    }
} else {
    Write-Host "  ✗ Visual Studio" -ForegroundColor Red -NoNewline
    Write-Host " - vswhere.exe 未找到" -ForegroundColor Gray
    $allOk = $false
}

# 检查项目文件
Write-Host "`n项目结构:" -ForegroundColor Yellow

$requiredPaths = @{
    "R0 项目" = "r0"
    "R3 项目" = "r3"
    "工具目录" = "tools"
    "CMakeLists" = "CMakeLists.txt"
}

foreach ($item in $requiredPaths.GetEnumerator()) {
    $path = Join-Path $script:RepoRoot $item.Value
    if (Test-Path $path) {
        Write-Host "  ✓ $($item.Key)" -ForegroundColor Green -NoNewline
        Write-Host " - $path" -ForegroundColor Gray
    } else {
        Write-Host "  ✗ $($item.Key)" -ForegroundColor Red -NoNewline
        Write-Host " - 不存在: $path" -ForegroundColor Gray
        $allOk = $false
    }
}

# 总结
Write-Host "`n============================================" -ForegroundColor Cyan
if ($allOk) {
    Write-Host "  ✓ 环境检查通过！" -ForegroundColor Green
} else {
    Write-Host "  ✗ 环境检查发现问题" -ForegroundColor Red
    Write-Host "  请安装缺失的组件" -ForegroundColor Yellow
}
Write-Host "============================================" -ForegroundColor Cyan
