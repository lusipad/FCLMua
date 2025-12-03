<#
.SYNOPSIS
    检查 MSVC 工具集兼容性
    
.DESCRIPTION
    验证项目是否可以使用特定版本的 MSVC 工具集编译
    
.PARAMETER TargetMSVCVersion
    目标 MSVC 版本（例如：14.38, 14.40, 14.44）
#>

param(
    [string]$TargetMSVCVersion = "14.38"
)

$ErrorActionPreference = 'Continue'

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  MSVC 工具集兼容性检查" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "目标 MSVC 版本: $TargetMSVCVersion" -ForegroundColor Yellow
Write-Host ""

# 1. 检查当前安装的 MSVC 版本
Write-Host "[1] 检查已安装的 MSVC 工具集..." -ForegroundColor Yellow
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (Test-Path $vsWherePath) {
    $vsPath = & $vsWherePath -version '[17.0,18.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath `
        -format value `
        -latest 2>$null
    
    if ($vsPath) {
        Write-Host "  Visual Studio 路径: $vsPath" -ForegroundColor Gray
        
        # 列出所有 MSVC 工具集
        $vcToolsPath = Join-Path $vsPath 'VC\Tools\MSVC'
        if (Test-Path $vcToolsPath) {
            $toolsets = Get-ChildItem $vcToolsPath -Directory | Sort-Object Name
            
            Write-Host "  可用的 MSVC 工具集:" -ForegroundColor Cyan
            $targetFound = $false
            foreach ($toolset in $toolsets) {
                $version = $toolset.Name
                $majorMinor = $version.Substring(0, 5)  # 例如 "14.38"
                
                if ($majorMinor -eq $TargetMSVCVersion) {
                    Write-Host "    ✓ $version (目标版本)" -ForegroundColor Green
                    $targetFound = $true
                    $script:TargetToolsetPath = $toolset.FullName
                } else {
                    Write-Host "    - $version" -ForegroundColor Gray
                }
            }
            
            if (-not $targetFound) {
                Write-Host ""
                Write-Host "  ⚠ 未找到 MSVC $TargetMSVCVersion" -ForegroundColor Yellow
                Write-Host "  你可以通过 Visual Studio Installer 安装旧版本的 MSVC 工具集" -ForegroundColor Yellow
            }
        }
    }
}

# 2. 检查项目的 C++ 标准要求
Write-Host ""
Write-Host "[2] 检查项目 C++ 标准要求..." -ForegroundColor Yellow
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'

if (Test-Path $cmakePath) {
    $cppStandard = Select-String -Path $cmakePath -Pattern "cxx_std_(\d+)" | ForEach-Object {
        $_.Matches.Groups[1].Value
    } | Select-Object -First 1
    
    if ($cppStandard) {
        Write-Host "  项目使用 C++ 标准: C++$cppStandard" -ForegroundColor Cyan
        
        # MSVC 14.38 的 C++ 标准支持
        $msvc1438Support = @{
            '14' = $true
            '17' = $true
            '20' = $true   # 大部分特性
            '23' = $false  # 部分特性
        }
        
        if ($msvc1438Support[$cppStandard]) {
            Write-Host "  ✓ MSVC $TargetMSVCVersion 完全支持 C++$cppStandard" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ MSVC $TargetMSVCVersion 可能不完全支持 C++$cppStandard" -ForegroundColor Yellow
        }
    }
}

# 3. 检查依赖库的编译器版本
Write-Host ""
Write-Host "[3] 检查依赖库..." -ForegroundColor Yellow

$musaRuntimePath = Join-Path $repoRoot 'external\Musa.Runtime\Publish'
$versionFile = Join-Path $musaRuntimePath '.version'

if (Test-Path $versionFile) {
    $musaVersion = Get-Content $versionFile
    Write-Host "  Musa.Runtime 版本: $musaVersion" -ForegroundColor Cyan
    
    # 尝试获取库的编译器信息
    $libPath = Join-Path $musaRuntimePath 'Library\Release\x64\Musa.Runtime.lib'
    if (Test-Path $libPath) {
        Write-Host "  ✓ 找到 Musa.Runtime.lib" -ForegroundColor Green
        
        # 使用 dumpbin 检查库信息（如果可用）
        $dumpbin = Get-Command dumpbin -ErrorAction SilentlyContinue
        if ($dumpbin) {
            Write-Host "  检查库的编译器版本..." -ForegroundColor Gray
            $dumpOutput = & dumpbin /HEADERS $libPath 2>$null | Select-String "Linker Version"
            if ($dumpOutput) {
                Write-Host "    $dumpOutput" -ForegroundColor Gray
            }
        }
    }
} else {
    Write-Host "  ⚠ Musa.Runtime 未安装" -ForegroundColor Yellow
}

# 4. ABI 兼容性说明
Write-Host ""
Write-Host "[4] ABI 兼容性说明" -ForegroundColor Yellow
Write-Host "  Visual Studio 2022 的不同小版本之间：" -ForegroundColor Cyan
Write-Host "    - C++ ABI 保持兼容（14.30 - 14.44）" -ForegroundColor Green
Write-Host "    - 可以混合使用不同小版本编译的库" -ForegroundColor Green
Write-Host "    - 建议：使用相同或相近的版本以获得最佳兼容性" -ForegroundColor Gray

# 5. 总结和建议
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  总结" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if ($targetFound) {
    Write-Host "✓ 你的系统有 MSVC $TargetMSVCVersion" -ForegroundColor Green
    Write-Host ""
    Write-Host "使用特定版本编译的方法：" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "方法 1: 设置环境变量（全局）" -ForegroundColor Cyan
    Write-Host "  `$env:VCToolsVersion = '$TargetMSVCVersion'" -ForegroundColor White
    Write-Host "  pwsh build.ps1" -ForegroundColor White
    Write-Host ""
    Write-Host "方法 2: MSBuild 参数（推荐）" -ForegroundColor Cyan
    Write-Host "  对于驱动项目，在 .vcxproj 中添加：" -ForegroundColor White
    Write-Host "  <PlatformToolsetVersion>$TargetMSVCVersion</PlatformToolsetVersion>" -ForegroundColor White
} else {
    Write-Host "⚠ 未找到 MSVC $TargetMSVCVersion" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "安装步骤：" -ForegroundColor Yellow
    Write-Host "  1. 打开 Visual Studio Installer" -ForegroundColor White
    Write-Host "  2. 点击 '修改'" -ForegroundColor White
    Write-Host "  3. 转到 '单个组件' 选项卡" -ForegroundColor White
    Write-Host "  4. 搜索 'MSVC v143 - VS 2022 C++ x64/x86 生成工具 (v14.38)'" -ForegroundColor White
    Write-Host "  5. 勾选并安装" -ForegroundColor White
}

Write-Host ""
