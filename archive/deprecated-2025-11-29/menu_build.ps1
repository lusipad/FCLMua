<#
.SYNOPSIS
    构建任务执行脚本

.DESCRIPTION
    专注于执行各种构建任务，整合了以下脚本的功能：
    - build_and_sign_driver.ps1
    - build_all.ps1
    - manual_build.cmd
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('R0-Debug','R0-Release','R3-Debug','R3-Release','All','CLI-Demo','GUI-Demo')]
    [string]$Task,
    
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    
    [switch]$NoSign,
    [switch]$Package,
    [string]$WdkVersion = $null
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$script:ToolsDir = Join-Path $script:RepoRoot 'tools'

# 导入公共函数库
$commonPath = Join-Path $script:ToolsDir 'common.psm1'
if (Test-Path $commonPath) {
    Import-Module $commonPath -Force
}

function Write-TaskHeader {
    param([string]$Title)
    Write-Host "`n============================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "============================================`n" -ForegroundColor Cyan
}

function Ensure-Directory {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Setup-Dependencies {
    Write-Host "Setting up dependencies..." -ForegroundColor Yellow
    $setupScript = Join-Path $script:ToolsDir 'setup_dependencies.ps1'
    if (Test-Path $setupScript) {
        & $setupScript
        $exitCode = $LASTEXITCODE
        if ($null -eq $exitCode) { $exitCode = 0 }
        if ($exitCode -ne 0) {
            throw "Dependency setup failed with exit code $exitCode"
        }
    }
    
    # Ensure Musa Runtime is published
    if (Get-Command 'Ensure-MusaRuntimePublish' -ErrorAction SilentlyContinue) {
        Ensure-MusaRuntimePublish -RepoRoot $script:RepoRoot
    }
}

function Build-R0Driver {
    param(
        [string]$Configuration,
        [bool]$Sign = $true
    )
    
    Write-TaskHeader "编译 R0 驱动 ($Configuration)$(if($Sign){' + 签名'})"
    
    $kernelDir = Join-Path $script:RepoRoot 'r0\driver\msbuild'
    $outputDir = Join-Path $kernelDir "out\$Platform\$Configuration"
    $driverSys = Join-Path $outputDir 'FclMusaDriver.sys'
    $driverPdb = Join-Path $outputDir 'FclMusaDriver.pdb'
    $distDir = Join-Path $script:RepoRoot "dist\driver\$Platform\$Configuration"
    
    # Setup dependencies
    Setup-Dependencies
    
    Write-Host "[1/$(if($Sign){'3'}else{'2'})] Building driver solution ($Configuration|$Platform)..." -ForegroundColor Yellow
    
    # Handle Musa Runtime configuration override for Debug builds
    $previousRuntimeOverride = $env:MUSA_RUNTIME_LIBRARY_CONFIGURATION
    $runtimeOverrideApplied = $false
    if ($Configuration -eq 'Debug') {
        $env:MUSA_RUNTIME_LIBRARY_CONFIGURATION = 'Release'
        $runtimeOverrideApplied = $true
    }
    
    try {
        # Call manual_build.ps1 (PowerShell version)
        # 使用点调用而不是 pwsh 子进程，避免退出码问题
        $buildScript = Join-Path $script:ToolsDir 'manual_build.ps1'
        
        # 使用 &Call operator 并捕获结果
        try {
            & $buildScript -Configuration $Configuration -Platform $Platform
        }
        catch {
            throw "manual_build.ps1 failed: $_"
        }
    }
    finally {
        if ($runtimeOverrideApplied) {
            if ([string]::IsNullOrWhiteSpace($previousRuntimeOverride)) {
                Remove-Item Env:MUSA_RUNTIME_LIBRARY_CONFIGURATION -ErrorAction SilentlyContinue
            } else {
                $env:MUSA_RUNTIME_LIBRARY_CONFIGURATION = $previousRuntimeOverride
            }
        }
    }
    
    if (-not (Test-Path $driverSys)) {
        throw "Driver SYS not found: $driverSys"
    }
    
    # Sign driver if requested
    if ($Sign) {
        Write-Host "[2/3] Signing driver: $driverSys" -ForegroundColor Yellow
        $signScript = Join-Path $script:ToolsDir 'sign_driver.ps1'
        & $signScript -DriverPath $driverSys
        
        $exitCode = $LASTEXITCODE
        if ($null -eq $exitCode) { $exitCode = 0 }
        
        if ($exitCode -ne 0) {
            throw "sign_driver.ps1 failed with exit code $exitCode"
        }
    }
    
    # Package artifacts
    $stepNum = if($Sign){'3'}else{'2'}
    Write-Host "[$stepNum/$stepNum] Packaging artifacts to $distDir" -ForegroundColor Yellow
    Ensure-Directory $distDir
    
    Copy-Item -Path $driverSys -Destination $distDir -Force
    if (Test-Path $driverPdb) {
        Copy-Item -Path $driverPdb -Destination $distDir -Force
    }
    
    # Copy certificates if they exist
    $cerPath = Join-Path $outputDir 'FclMusaTestCert.cer'
    $pfxPath = Join-Path $outputDir 'FclMusaTestCert.pfx'
    if (Test-Path $cerPath) {
        Copy-Item -Path $cerPath -Destination $distDir -Force
    }
    if (Test-Path $pfxPath) {
        Copy-Item -Path $pfxPath -Destination $distDir -Force
    }
    
    Write-Host "`n✓ R0 驱动编译$(if($Sign){'并签名'})成功 ($Configuration)" -ForegroundColor Green
    Write-Host "  输出目录: $distDir" -ForegroundColor Gray
}

function Build-R3Demo {
    param([string]$Configuration)
    
    Write-TaskHeader "编译 R3 Demo ($Configuration)"
    
    $buildDir = Join-Path $script:RepoRoot "build\r3-demo"
    
    Write-Host "运行 CMake 配置..." -ForegroundColor Yellow
    
    # CMake configuration for R3 demo
    $cmakeArgs = @(
        '-S', $script:RepoRoot,
        '-B', $buildDir,
        '-G', 'Visual Studio 17 2022',
        '-A', 'x64',
        '-DFCLMUSA_BUILD_DRIVER=OFF',
        '-DFCLMUSA_BUILD_KERNEL_LIB=OFF',
        '-DFCLMUSA_BUILD_USERLIB=ON'
    )
    
    & cmake @cmakeArgs
    $exitCode = $LASTEXITCODE
    if ($null -eq $exitCode) { $exitCode = 0 }
    
    if ($exitCode -ne 0) {
        throw "CMake 配置失败 (exit code: $exitCode)"
    }
    
    Write-Host "`n运行 CMake 编译..." -ForegroundColor Yellow
    cmake --build $buildDir --config $Configuration --target FclMusaUserDemo -j $env:NUMBER_OF_PROCESSORS
    $exitCode = $LASTEXITCODE
    if ($null -eq $exitCode) { $exitCode = 0 }
    
    if ($exitCode -ne 0) {
        throw "CMake 编译失败 (exit code: $exitCode)"
    }
    
    Write-Host "`n✓ R3 Demo 编译成功 ($Configuration)" -ForegroundColor Green
    
    # Show output path
    $demoExe = Join-Path (Join-Path $buildDir $Configuration) 'FclMusaUserDemo.exe'
    if (Test-Path $demoExe) {
        Write-Host "  可执行文件: $demoExe" -ForegroundColor Gray
    }
}

function Build-CLIDemo {
    param([string]$Configuration = 'Release')
    
    Write-TaskHeader "编译 CLI Demo ($Configuration)"
    
    $buildAllScript = Join-Path $script:ToolsDir 'build_all.ps1'
    if (-not (Test-Path $buildAllScript)) {
        Write-Host "⚠ build_all.ps1 not found, skipping CLI Demo" -ForegroundColor Yellow
        return
    }
    
    $buildArgs = @{
        Configuration = $Configuration
        SkipDriver = $true
        SkipGUI = $true
    }
    
    & $buildAllScript @buildArgs
    $exitCode = $LASTEXITCODE
    if ($null -eq $exitCode) { $exitCode = 0 }
    
    if ($exitCode -eq 0) {
        Write-Host "`n✓ CLI Demo 编译成功 ($Configuration)" -ForegroundColor Green
    } else {
        throw "CLI Demo 编译失败 (exit code: $exitCode)"
    }
}

function Build-GUIDemo {
    param([string]$Configuration = 'Release')
    
    Write-TaskHeader "编译 GUI Demo ($Configuration)"
    
    $buildAllScript = Join-Path $script:ToolsDir 'build_all.ps1'
    if (-not (Test-Path $buildAllScript)) {
        Write-Host "⚠ build_all.ps1 not found, skipping GUI Demo" -ForegroundColor Yellow
        return
    }
    
    $buildArgs = @{
        Configuration = $Configuration
        SkipDriver = $true
        SkipCLI = $true
    }
    
    & $buildAllScript @buildArgs
    $exitCode = $LASTEXITCODE
    if ($null -eq $exitCode) { $exitCode = 0 }
    
    if ($exitCode -eq 0) {
        Write-Host "`n✓ GUI Demo 编译成功 ($Configuration)" -ForegroundColor Green
    } else {
        throw "GUI Demo 编译失败 (exit code: $exitCode)"
    }
}

function Build-All {
    Write-TaskHeader "编译所有项目"
    
    try {
        Write-Host "编译顺序: R0 Debug → R0 Release → R3 Debug → R3 Release`n" -ForegroundColor Cyan
        
        Build-R0Driver 'Debug' -Sign (-not $NoSign)
        Build-R0Driver 'Release' -Sign (-not $NoSign)
        Build-R3Demo 'Debug'
        Build-R3Demo 'Release'
        
        if ($Package) {
            Write-TaskHeader "打包发布产物"
            Package-Artifacts
        }
        
        Write-Host "`n============================================" -ForegroundColor Green
        Write-Host "  ✓ 所有项目编译成功！" -ForegroundColor Green
        Write-Host "============================================" -ForegroundColor Green
    }
    catch {
        Write-Host "`n✗ 编译失败: $_" -ForegroundColor Red
        throw
    }
}

function Package-Artifacts {
    $packageScript = Join-Path $script:ToolsDir 'package_bundle.ps1'
    if (Test-Path $packageScript) {
        Write-Host "运行打包脚本..." -ForegroundColor Yellow
        & $packageScript
        $exitCode = $LASTEXITCODE
        if ($null -eq $exitCode) { $exitCode = 0 }
        
        if ($exitCode -eq 0) {
            Write-Host "✓ 打包完成" -ForegroundColor Green
        } else {
            Write-Host "⚠ 打包失败 (exit code: $exitCode)" -ForegroundColor Yellow
        }
    } else {
        Write-Host "⚠ 打包脚本不存在: $packageScript" -ForegroundColor Yellow
    }
}

# 执行任务
try {
    switch ($Task) {
        'R0-Debug'   { Build-R0Driver 'Debug' -Sign (-not $NoSign) }
        'R0-Release' { Build-R0Driver 'Release' -Sign (-not $NoSign) }
        'R3-Debug'   { Build-R3Demo 'Debug' }
        'R3-Release' { Build-R3Demo 'Release' }
        'CLI-Demo'   { Build-CLIDemo 'Release' }
        'GUI-Demo'   { Build-GUIDemo 'Release' }
        'All'        { Build-All }
    }
    
    # 成功时等待回车
    Write-Host ""
    Read-Host "按 Enter 继续"
}
catch {
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Red
    Write-Host "  ✗ 构建任务失败" -ForegroundColor Red
    Write-Host "============================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "错误: $_" -ForegroundColor Red
    Write-Host ""
    
    # 失败时等待回车
    Read-Host "按 Enter 返回菜单"
    
    exit 1
}
