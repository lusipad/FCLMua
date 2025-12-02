<#
.SYNOPSIS
    诊断 FCL+Musa 构建问题
    
.DESCRIPTION
    检查常见的构建问题并提供解决方案
#>

$ErrorActionPreference = 'Continue'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  FCL+Musa 构建问题诊断工具" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 1. 检查 NuGet 全局包
Write-Host "[1] 检查 NuGet 全局包..." -ForegroundColor Yellow
$nugetGlobalPath = Join-Path $env:USERPROFILE '.nuget\packages'
Write-Host "  NuGet 全局包路径: $nugetGlobalPath"

$requiredPackages = @(
    @{Name='Musa.Core'; Version='0.4.1'},
    @{Name='Musa.CoreLite'; Version='1.0.3'},
    @{Name='Musa.Veil'; Version='1.5.0'}
)

$missingPackages = @()
foreach ($pkg in $requiredPackages) {
    $possiblePaths = @(
        (Join-Path $nugetGlobalPath "$($pkg.Name)\$($pkg.Version)"),
        (Join-Path $nugetGlobalPath "$($pkg.Name.ToLower())\$($pkg.Version)"),
        (Join-Path $nugetGlobalPath "$($pkg.Name).$($pkg.Version)"),
        (Join-Path $nugetGlobalPath "$($pkg.Name.ToLower()).$($pkg.Version)")
    )
    
    $found = $false
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            Write-Host "  ✓ $($pkg.Name) $($pkg.Version) - $path" -ForegroundColor Green
            $found = $true
            break
        }
    }
    
    if (-not $found) {
        Write-Host "  ✗ $($pkg.Name) $($pkg.Version) - 未找到" -ForegroundColor Red
        $missingPackages += $pkg
    }
}

if ($missingPackages.Count -gt 0) {
    Write-Host ""
    Write-Host "  建议：运行以下命令恢复 NuGet 包" -ForegroundColor Yellow
    Write-Host "  pwsh tools\scripts\restore_kernel_packages.ps1" -ForegroundColor White
}

# 2. 检查 Musa.Runtime
Write-Host ""
Write-Host "[2] 检查 Musa.Runtime..." -ForegroundColor Yellow
$musaRuntimePublish = Join-Path $repoRoot 'external\Musa.Runtime\Publish'
$versionFile = Join-Path $musaRuntimePublish '.version'

if (Test-Path $versionFile) {
    $version = Get-Content $versionFile
    Write-Host "  ✓ Musa.Runtime 版本: $version" -ForegroundColor Green
    Write-Host "    路径: $musaRuntimePublish"
    
    # 检查关键文件
    $musaRuntimeProps = Join-Path $musaRuntimePublish 'Musa.Runtime.props'
    if (Test-Path $musaRuntimeProps) {
        Write-Host "  ✓ Musa.Runtime.props 存在" -ForegroundColor Green
        
        # 检查 MUSA_RUNTIME_LIBRARY_CONFIGURATION 配置
        $propsContent = Get-Content $musaRuntimeProps -Raw
        if ($propsContent -match 'MUSA_RUNTIME_LIBRARY_CONFIGURATION') {
            Write-Host "  ✓ MUSA_RUNTIME_LIBRARY_CONFIGURATION 已配置" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ MUSA_RUNTIME_LIBRARY_CONFIGURATION 未配置" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ✗ Musa.Runtime.props 不存在" -ForegroundColor Red
    }
    
    # 检查库文件
    $libPaths = @(
        (Join-Path $musaRuntimePublish 'Library\Release\x64\Musa.Runtime.lib'),
        (Join-Path $musaRuntimePublish 'Library\Debug\x64\Musa.Runtime.lib')
    )
    
    foreach ($libPath in $libPaths) {
        if (Test-Path $libPath) {
            $config = if ($libPath -match 'Release') { 'Release' } else { 'Debug' }
            Write-Host "  ✓ Musa.Runtime.lib ($config) 存在" -ForegroundColor Green
        }
    }
    
} else {
    Write-Host "  ✗ Musa.Runtime 未安装" -ForegroundColor Red
    Write-Host "  建议：运行以下命令安装 Musa.Runtime" -ForegroundColor Yellow
    Write-Host "  pwsh tools\scripts\setup_dependencies.ps1" -ForegroundColor White
}

# 3. 检查 Visual Studio 2022
Write-Host ""
Write-Host "[3] 检查 Visual Studio 2022..." -ForegroundColor Yellow
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (Test-Path $vsWherePath) {
    $vsPath = & $vsWherePath -version '[17.0,18.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath `
        -format value `
        -latest 2>$null
    
    if ($vsPath) {
        $vsVersion = & $vsWherePath -version '[17.0,18.0)' `
            -property installationVersion `
            -format value `
            -latest 2>$null
        Write-Host "  ✓ Visual Studio 2022 已安装" -ForegroundColor Green
        Write-Host "    路径: $vsPath"
        Write-Host "    版本: $vsVersion"
    } else {
        Write-Host "  ✗ Visual Studio 2022 未找到" -ForegroundColor Red
        Write-Host "  建议：安装 Visual Studio 2022" -ForegroundColor Yellow
        Write-Host "  https://visualstudio.microsoft.com/" -ForegroundColor White
    }
} else {
    Write-Host "  ⚠ vswhere.exe 未找到" -ForegroundColor Yellow
}

# 4. 检查 WDK
Write-Host ""
Write-Host "[4] 检查 WDK..." -ForegroundColor Yellow
$wdkRoot = if ($env:WDKContentRoot) { $env:WDKContentRoot } else { 'C:\Program Files (x86)\Windows Kits\10' }

if (Test-Path $wdkRoot) {
    Write-Host "  ✓ WDK 根目录: $wdkRoot" -ForegroundColor Green
    
    $includePath = Join-Path $wdkRoot 'Include'
    if (Test-Path $includePath) {
        $versions = Get-ChildItem -Path $includePath -Directory | 
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
            Sort-Object -Property Name -Descending |
            Select-Object -First 3
        
        Write-Host "  可用的 WDK 版本:"
        foreach ($ver in $versions) {
            $kmPath = Join-Path $ver.FullName 'km'
            if (Test-Path $kmPath) {
                Write-Host "    ✓ $($ver.Name) (含内核模式头文件)" -ForegroundColor Green
            } else {
                Write-Host "    ⚠ $($ver.Name) (缺少内核模式头文件)" -ForegroundColor Yellow
            }
        }
    }
} else {
    Write-Host "  ✗ WDK 未安装" -ForegroundColor Red
    Write-Host "  建议：安装 Windows Driver Kit (WDK)" -ForegroundColor Yellow
}

# 5. 检查 FCL 补丁
Write-Host ""
Write-Host "[5] 检查 FCL Kernel 模式补丁..." -ForegroundColor Yellow
$fclSourcePath = Join-Path $repoRoot 'external\fcl-source'
if (Test-Path $fclSourcePath) {
    Push-Location $fclSourcePath
    try {
        # 检查是否有未提交的更改（补丁应用后会有）
        $status = git status --porcelain 2>$null
        if ($status) {
            Write-Host "  ✓ FCL 源码有修改（补丁可能已应用）" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ FCL 源码无修改（补丁可能未应用）" -ForegroundColor Yellow
            Write-Host "  建议：运行以下命令应用补丁" -ForegroundColor Yellow
            Write-Host "  pwsh tools\scripts\apply_fcl_patch.ps1" -ForegroundColor White
        }
    }
    finally {
        Pop-Location
    }
} else {
    Write-Host "  ✗ FCL 源码子模块未初始化" -ForegroundColor Red
    Write-Host "  建议：运行 git submodule update --init --recursive" -ForegroundColor Yellow
}

# 6. 总结
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  诊断完成" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if ($missingPackages.Count -eq 0 -and (Test-Path $versionFile)) {
    Write-Host "✓ 未发现明显问题，构建环境看起来正常" -ForegroundColor Green
} else {
    Write-Host "⚠ 发现一些问题，请按照上述建议进行修复" -ForegroundColor Yellow
}

Write-Host ""
