<#
.SYNOPSIS
    构建任务执行脚本
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('R0-Debug','R0-Release','R3-Lib-Debug','R3-Lib-Release','R3-Demo-Debug','R3-Demo-Release','CLI-Demo','GUI-Demo','All')]
    [string]$Task
)

$ErrorActionPreference = 'Stop'

Write-Host "Loading common module..." -ForegroundColor Cyan
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
Write-Host "  Module loaded successfully" -ForegroundColor Green

Write-Host "Finding repository root..." -ForegroundColor Cyan
$script:RepoRoot = Get-FCLRepoRoot
Write-Host "  Repository root: $script:RepoRoot" -ForegroundColor Green

function Ensure-FCLDriverSolution {
    $solutionPath = Join-Path $script:RepoRoot 'kernel\driver\msbuild\FclMusaDriver.sln'
    if (-not (Test-Path $solutionPath)) {
        throw "Driver solution not found at $solutionPath. Please ensure the repository is properly cloned with all files."
    }
    return $solutionPath
}

function Build-R0Driver {
    param(
        [string]$Configuration
    )
    
    Write-FCLHeader "编译 R0 驱动 ($Configuration) + 签名"
    
    # Setup dependencies
    Setup-FCLDependencies $script:RepoRoot
    
    # Find WDK
    Write-Host "Locating WDK..." -ForegroundColor Yellow
    $wdk = Find-FCLWDK
    Write-Host "  WDK Version: $($wdk.Version)" -ForegroundColor Green
    
    # Build driver
    $solutionPath = Ensure-FCLDriverSolution
    $properties = @{
        WindowsTargetPlatformVersion = $wdk.Version
    }
    
    # For Debug builds, use Release version of Musa.Runtime
    if ($Configuration -eq 'Debug') {
        $properties['MUSA_RUNTIME_LIBRARY_CONFIGURATION'] = 'Release'
    }
    
    # Verify Musa.Runtime is installed before building
    $musaRuntimeCheck = Join-Path $script:RepoRoot 'external\Musa.Runtime\Publish\.version'
    if (-not (Test-Path $musaRuntimeCheck)) {
        Write-Error "ERROR: Musa.Runtime not installed! File not found: $musaRuntimeCheck"
        Write-Error "Setup-FCLDependencies should have installed it."
        throw "Musa.Runtime is required but not installed"
    }
    $musaVersion = Get-Content $musaRuntimeCheck -Raw
    Write-Host "Using Musa.Runtime version: $($musaVersion.Trim())" -ForegroundColor Cyan
    
    # Verify critical Musa.Runtime files
    $musaConfigProps = Join-Path $script:RepoRoot 'external\Musa.Runtime\Publish\Config\Musa.Runtime.Config.props'
    if (-not (Test-Path $musaConfigProps)) {
        Write-Error "ERROR: Musa.Runtime.Config.props not found at $musaConfigProps"
        Write-Host "Contents of Publish directory:" -ForegroundColor Yellow
        Get-ChildItem (Join-Path $script:RepoRoot 'external\Musa.Runtime\Publish') -Recurse | ForEach-Object { Write-Host "  $($_.FullName)" }
        throw "Musa.Runtime.Config.props is missing"
    }
    Write-Host "✓ Musa.Runtime.Config.props found" -ForegroundColor Green
    
    Invoke-FCLMsBuild -SolutionPath $solutionPath `
        -Configuration $Configuration `
        -Platform 'x64' `
        -Properties $properties `
        -Targets @('Clean','Build')
    
    # Verify driver was built
    $outputDir = Join-Path $script:RepoRoot "kernel\driver\msbuild\out\x64\$Configuration"
    $driverSys = Join-Path $outputDir 'FclMusaDriver.sys'
    
    if (-not (Test-Path $driverSys)) {
        throw "Driver not found: $driverSys"
    }
    
    # Sign driver (skip in CI environment)
    if ($env:CI -eq 'true') {
        Write-Host "Skipping driver signing in CI environment" -ForegroundColor Yellow
    } else {
        Write-Host "Signing driver..." -ForegroundColor Yellow
        $signScript = Join-Path $script:RepoRoot 'tools\sign_driver.ps1'
        & pwsh -NoProfile -ExecutionPolicy Bypass -File $signScript -DriverPath $driverSys
        
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Driver signing failed (continuing without signature)"
        }
    }
    
    # Copy to dist
    $distDir = Join-Path $script:RepoRoot "dist\driver\x64\$Configuration"
    if (-not (Test-Path $distDir)) {
        New-Item -ItemType Directory -Path $distDir -Force | Out-Null
    }
    
    Write-Host "Copying to dist..." -ForegroundColor Yellow
    Copy-Item $driverSys -Destination $distDir -Force
    
    $pdbPath = Join-Path $outputDir 'FclMusaDriver.pdb'
    if (Test-Path $pdbPath) {
        Copy-Item $pdbPath -Destination $distDir -Force
    }
    
    $cerPath = Join-Path $outputDir 'FclMusaTestCert.cer'
    if (Test-Path $cerPath) {
        Copy-Item $cerPath -Destination $distDir -Force
    }
    
    Write-FCLSuccess "R0 驱动编译并签名成功 ($Configuration)"
    Write-Host "  输出: $distDir" -ForegroundColor Gray
}

function Build-R3Lib {
    param([string]$Configuration)
    Write-FCLHeader "编译 R3 用户态库 ($Configuration)"
    $buildDir = Join-Path $script:RepoRoot "build\r3-lib"
    $sourceDir = $script:RepoRoot
    $options = @{
        'FCLMUSA_BUILD_DRIVER' = 'OFF'
        'FCLMUSA_BUILD_KERNEL_LIB' = 'OFF'
        'FCLMUSA_BUILD_USERLIB' = 'ON'
    }
    Invoke-FCLCMake -SourceDir $sourceDir -BuildDir $buildDir -Configuration $Configuration -Options $options
    Write-Host "  仅构建库目标 FclMusaCoreUser..." -ForegroundColor Yellow
    & cmake --build $buildDir --config $Configuration --target FclMusaCoreUser -j $env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) { throw "R3 lib build failed" }
    $libPath = Join-Path $buildDir "$Configuration\FclMusaCoreUser.lib"
    if (Test-Path $libPath) {
        $distDir = Join-Path $script:RepoRoot "dist\lib\r3\$Configuration"
        if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir -Force | Out-Null }
        Copy-Item $libPath -Destination $distDir -Force
        Write-Host "  输出: $distDir" -ForegroundColor Gray
    }
    Write-FCLSuccess "R3 用户态库编译成功 ($Configuration)"
}

function Build-R3Demo {
    param([string]$Configuration)
    Write-FCLHeader "编译 R3 Demo ($Configuration)"
    $buildDir = Join-Path $script:RepoRoot "build\r3-demo"
    $sourceDir = $script:RepoRoot
    $options = @{
        'FCLMUSA_BUILD_DRIVER' = 'OFF'
        'FCLMUSA_BUILD_KERNEL_LIB' = 'OFF'
        'FCLMUSA_BUILD_USERLIB' = 'ON'
    }
    Invoke-FCLCMake -SourceDir $sourceDir -BuildDir $buildDir -Configuration $Configuration -Options $options
    Write-Host "  构建 Demo 目标 FclMusaUserDemo..." -ForegroundColor Yellow
    & cmake --build $buildDir --config $Configuration --target FclMusaUserDemo -j $env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) { throw "R3 demo build failed" }
    $exePath = Join-Path $buildDir "$Configuration\FclMusaUserDemo.exe"
    if (Test-Path $exePath) { Write-Host "  可执行文件: $exePath" -ForegroundColor Gray }
    Write-FCLSuccess "R3 Demo 编译成功 ($Configuration)"
}

function Build-CLIDemo {
    Write-FCLHeader "编译 CLI Demo (Release)"
    
    $cliDemoDir = Join-Path $script:RepoRoot 'samples\cli_demo'
    $compileCmd = Join-Path $cliDemoDir 'compile.cmd'
    
    if (-not (Test-Path $compileCmd)) {
        throw "compile.cmd not found in $cliDemoDir"
    }
    
    Push-Location $cliDemoDir
    try {
        & cmd /c compile.cmd
        if ($LASTEXITCODE -ne 0) {
            throw "CLI Demo build failed"
        }
    }
    finally {
        Pop-Location
    }
    
    Write-FCLSuccess "CLI Demo 编译成功 (Release)"
}

function Build-GUIDemo {
    Write-FCLHeader "编译 GUI Demo (Release)"
    
    $guiDemoDir = Join-Path $script:RepoRoot 'samples\gui_demo'
    $vcxproj = Join-Path $guiDemoDir 'FclGuiDemo.vcxproj'
    
    if (-not (Test-Path $vcxproj)) {
        throw "FclGuiDemo.vcxproj not found in $guiDemoDir"
    }
    
    # Build using MSBuild
    Invoke-FCLMsBuild -SolutionPath $vcxproj `
        -Configuration 'Release' `
        -Platform 'x64' `
        -Targets @('Build')
    
    Write-FCLSuccess "GUI Demo 编译成功 (Release)"
}

function Build-All {
    Write-FCLHeader "编译所有项目"
    
    Build-R0Driver 'Debug'
    Build-R0Driver 'Release'
    Build-R3Demo 'Debug'
    Build-R3Demo 'Release'
    Build-CLIDemo
    Build-GUIDemo
    
    Write-FCLSuccess "所有项目编译成功！"
}

# Execute task
switch ($Task) {
    'R0-Debug'        { Build-R0Driver 'Debug' }
    'R0-Release'      { Build-R0Driver 'Release' }
    'R3-Lib-Debug'    { Build-R3Lib 'Debug' }
    'R3-Lib-Release'  { Build-R3Lib 'Release' }
    'R3-Demo-Debug'   { Build-R3Demo 'Debug' }
    'R3-Demo-Release' { Build-R3Demo 'Release' }
    'CLI-Demo'        { Build-CLIDemo }
    'GUI-Demo'        { Build-GUIDemo }
    'All'             { Build-All }
}
