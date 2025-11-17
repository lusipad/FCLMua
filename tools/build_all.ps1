<#
.SYNOPSIS
    FCL+Musa 统一构建脚本

.DESCRIPTION
    统一的构建脚本，支持所有构建场景：
    - 仅构建驱动
    - 构建驱动并签名
    - 构建所有组件（驱动 + CLI Demo + GUI Demo）
    - 打包发布包

    替代了之前的三个脚本：
    - build_all.ps1（旧）
    - build_and_sign_driver.ps1
    - build_all_and_sign.ps1（旧）

.PARAMETER Configuration
    构建配置：Debug 或 Release（默认：Debug）

.PARAMETER Platform
    目标平台：x64（默认：x64）

.PARAMETER Sign
    是否签名驱动（默认：否）

.PARAMETER DriverOnly
    仅构建驱动，不构建 Demo

.PARAMETER SkipDriver
    跳过驱动构建，仅构建 Demo

.PARAMETER SkipCLI
    跳过 CLI Demo 构建

.PARAMETER SkipGUI
    跳过 GUI Demo 构建

.PARAMETER Package
    打包所有产物到 dist/bundle/

.EXAMPLE
    .\build_all.ps1
    构建所有组件（Debug，不签名）

.EXAMPLE
    .\build_all.ps1 -Configuration Release -Sign
    构建所有组件（Release）并签名驱动

.EXAMPLE
    .\build_all.ps1 -DriverOnly -Sign
    仅构建并签名驱动

.EXAMPLE
    .\build_all.ps1 -Configuration Release -Sign -Package
    构建所有组件、签名驱动并打包（完整发布流程）

.EXAMPLE
    .\build_all.ps1 -SkipGUI
    构建驱动和 CLI Demo，跳过 GUI Demo

.NOTES
    依赖：
    - Visual Studio 2022/2019
    - WDK 10.0.26100.0
    - Musa.Runtime（仓库自带）
#>

[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [switch]$Sign,
    [switch]$DriverOnly,
    [switch]$SkipDriver,
    [switch]$SkipCLI,
    [switch]$SkipGUI,
    [switch]$Package
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 导入公共函数库
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$commonPath = Join-Path $scriptDir 'common.ps1'
if (-not (Test-Path -Path $commonPath)) {
    throw "Common functions module not found: $commonPath"
}
Import-Module $commonPath -Force

# 计算仓库根目录和关键路径
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..')).ProviderPath
$kernelDir = Join-Path $repoRoot 'kernel/FclMusaDriver'
$outputDir = Join-Path $kernelDir "out\$Platform\$Configuration"
$driverSys = Join-Path $outputDir 'FclMusaDriver.sys'
$driverPdb = Join-Path $outputDir 'FclMusaDriver.pdb'
$distDriverDir = Join-Path $repoRoot "dist\driver\$Platform\$Configuration"

# 参数验证
if ($DriverOnly -and $SkipDriver) {
    throw "Cannot specify both -DriverOnly and -SkipDriver"
}

if ($DriverOnly -and ($SkipCLI -or $SkipGUI)) {
    Write-BuildWarning "-DriverOnly overrides -SkipCLI and -SkipGUI"
}

if ($SkipDriver -and $Sign) {
    throw "Cannot sign driver when -SkipDriver is specified"
}

# 计算要执行的步骤
$steps = @()
if (-not $SkipDriver) {
    $steps += "Driver"
    if ($Sign) {
        $steps += "Sign"
    }
}
if (-not $DriverOnly -and -not $SkipCLI) {
    $steps += "CLI"
}
if (-not $DriverOnly -and -not $SkipGUI) {
    $steps += "GUI"
}
if ($Package) {
    $steps += "Package"
}

$totalSteps = $steps.Count
$currentStep = 0

Write-Host ""
Write-Host "FCL+Musa Build System" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration | $Platform" -ForegroundColor Gray
Write-Host "Steps:         $($steps -join ' → ')" -ForegroundColor Gray
Write-Host ""

# ========================================
# 步骤 1: 构建驱动
# ========================================
if ($steps -contains "Driver") {
    $currentStep++
    $signText = if ($Sign) { " + sign" } else { "" }
    Write-BuildStep $currentStep $totalSteps "Building kernel driver$signText ($Configuration|$Platform)"

    # 确保 Musa.Runtime 配置存在
    Ensure-MusaRuntimePublish -RepoRoot $repoRoot

    # 调用 manual_build.cmd
    $manualBuildCmd = Join-Path $scriptDir 'manual_build.cmd'
    Invoke-BuildCommand `
        -ScriptBlock { & $manualBuildCmd $Configuration } `
        -ErrorMessage "manual_build.cmd failed. See kernel\FclMusaDriver\build_manual_build.log for details."

    # 验证驱动文件存在
    if (-not (Test-Path -Path $driverSys -PathType Leaf)) {
        throw "Driver SYS not found after build: $driverSys"
    }

    Write-Host "  ✓ Driver built: $driverSys" -ForegroundColor Green
}

# ========================================
# 步骤 2: 签名驱动（如果需要）
# ========================================
if ($steps -contains "Sign") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Signing driver"

    $signDriverScript = Join-Path $scriptDir 'sign_driver.ps1'
    Invoke-BuildCommand `
        -ScriptBlock { & $signDriverScript -DriverPath $driverSys } `
        -ErrorMessage "sign_driver.ps1 failed"

    Write-Host "  ✓ Driver signed" -ForegroundColor Green

    # 复制到 dist/ 目录
    Write-Host "  → Packaging to $distDriverDir" -ForegroundColor Gray
    Ensure-Directory $distDriverDir

    Copy-Item -Path $driverSys -Destination $distDriverDir -Force
    Copy-IfExists -Source $driverPdb -Destination $distDriverDir -Force | Out-Null

    $cerPath = Join-Path $outputDir 'FclMusaTestCert.cer'
    $pfxPath = Join-Path $outputDir 'FclMusaTestCert.pfx'
    Copy-IfExists -Source $cerPath -Destination $distDriverDir -Force | Out-Null
    Copy-IfExists -Source $pfxPath -Destination $distDriverDir -Force | Out-Null

    Write-Host "  ✓ Artifacts packaged to dist/driver/" -ForegroundColor Green
}

# ========================================
# 步骤 3: 构建 CLI Demo
# ========================================
if ($steps -contains "CLI") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Building CLI demo"

    $buildDemoCmd = Join-Path $scriptDir 'build_demo.cmd'
    Invoke-BuildCommand `
        -ScriptBlock { & $buildDemoCmd } `
        -ErrorMessage "build_demo.cmd failed"

    $cliExe = Join-Path $scriptDir 'build/fcl_demo.exe'
    if (Test-Path -Path $cliExe -PathType Leaf) {
        Write-Host "  ✓ CLI demo built: $cliExe" -ForegroundColor Green
    } else {
        Write-BuildWarning "CLI demo executable not found: $cliExe"
    }
}

# ========================================
# 步骤 4: 构建 GUI Demo
# ========================================
if ($steps -contains "GUI") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Building GUI demo"

    # 动态查找 MSBuild（不再硬编码）
    $msbuild = Get-MSBuildPath

    $guiProject = Join-Path $scriptDir 'gui_demo\FclGuiDemo.vcxproj'
    Invoke-BuildCommand `
        -ScriptBlock {
            & $msbuild $guiProject `
                /p:Configuration=Release `
                /p:Platform=$Platform `
                /m `
                /nologo
        } `
        -ErrorMessage "MSBuild for FclGuiDemo.vcxproj failed"

    $guiExe = Join-Path $scriptDir "gui_demo\release\FclGuiDemo.exe"
    if (Test-Path -Path $guiExe -PathType Leaf) {
        Write-Host "  ✓ GUI demo built: $guiExe" -ForegroundColor Green
    } else {
        Write-BuildWarning "GUI demo executable not found: $guiExe"
    }
}

# ========================================
# 步骤 5: 打包（如果需要）
# ========================================
if ($steps -contains "Package") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Packaging bundle"

    $packageScript = Join-Path $scriptDir 'package_bundle.ps1'
    Invoke-BuildCommand `
        -ScriptBlock { & $packageScript -Configuration $Configuration -Platform $Platform } `
        -ErrorMessage "package_bundle.ps1 failed"

    $bundleDir = Join-Path $repoRoot "dist\bundle\$Platform\$Configuration"
    Write-Host "  ✓ Bundle packaged: $bundleDir" -ForegroundColor Green
}

# ========================================
# 构建完成总结
# ========================================
Write-Host ""
Write-Success "✓ Build completed successfully!"
Write-Host ""

# 输出构建产物位置
if ($steps -contains "Driver" -or $steps -contains "Sign") {
    $driverLocation = if ($steps -contains "Sign") {
        "dist\driver\$Platform\$Configuration\FclMusaDriver.sys (signed)"
    } else {
        "$outputDir\FclMusaDriver.sys (unsigned)"
    }
    Write-Host "Driver:  $driverLocation" -ForegroundColor Gray
}

if ($steps -contains "CLI") {
    Write-Host "CLI Demo: tools\build\fcl_demo.exe" -ForegroundColor Gray
}

if ($steps -contains "GUI") {
    Write-Host "GUI Demo: tools\gui_demo\release\FclGuiDemo.exe" -ForegroundColor Gray
}

if ($steps -contains "Package") {
    Write-Host "Bundle:   dist\bundle\$Platform\$Configuration\" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
if ($steps -contains "Driver") {
    Write-Host "  1. Install driver: tools\manage_driver.ps1 -Action Install" -ForegroundColor Gray
    Write-Host "  2. Start driver:   tools\manage_driver.ps1 -Action Start" -ForegroundColor Gray
    Write-Host "  3. Run self-test:  tools\fcl-self-test.ps1" -ForegroundColor Gray
}
if ($steps -contains "CLI") {
    Write-Host "  4. Run CLI demo:   tools\build\fcl_demo.exe" -ForegroundColor Gray
}
Write-Host ""
