<#
.SYNOPSIS
    测试任务执行脚本

.DESCRIPTION
    专注于执行各种测试任务，整合了以下脚本的功能：
    - fcl-self-test.ps1
    - run_all_tests.ps1
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('R0-Demo','R3-Demo','GUI-Demo','All-Tests','R3-CTest','FCL-Test','Eigen-Test')]
    [string]$Task
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$script:ToolsDir = Join-Path $script:RepoRoot 'tools'

# Test statistics
$script:TotalTests = 0
$script:PassedTests = 0
$script:FailedTests = 0

function Write-TaskHeader {
    param([string]$Title)
    Write-Host "`n============================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "============================================`n" -ForegroundColor Cyan
}

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed
    )
    
    $script:TotalTests++
    
    if ($Passed) {
        $script:PassedTests++
        Write-Host "  ✓ $TestName" -ForegroundColor Green
    } else {
        $script:FailedTests++
        Write-Host "  ✗ $TestName" -ForegroundColor Red
    }
}

function Show-TestSummary {
    Write-Host "`n============================================" -ForegroundColor Cyan
    Write-Host "  测试总结" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  总计: $script:TotalTests" -ForegroundColor Gray
    Write-Host "  通过: $script:PassedTests" -ForegroundColor Green
    Write-Host "  失败: $script:FailedTests" -ForegroundColor Red
    Write-Host "============================================`n" -ForegroundColor Cyan
}

function Test-R0Demo {
    Write-TaskHeader "运行 R0 驱动测试"
    
    $testScript = Join-Path $script:ToolsDir 'fcl-self-test.ps1'
    
    if (-not (Test-Path $testScript)) {
        Write-Host "✗ 测试脚本不存在: $testScript" -ForegroundColor Red
        return
    }
    
    & $testScript
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n✓ R0 驱动测试完成" -ForegroundColor Green
        Write-TestResult "R0 驱动测试" $true
    } else {
        Write-Host "`n✗ R0 驱动测试失败" -ForegroundColor Red
        Write-TestResult "R0 驱动测试" $false
    }
}

function Test-R3Demo {
    Write-TaskHeader "运行 R3 Demo"
    
    # 尝试查找编译好的 R3 demo
    $possiblePaths = @(
        "build\r3-demo\Debug\FclMusaUserDemo.exe",
        "build\r3-demo\Release\FclMusaUserDemo.exe",
        "build-r3-debug\Debug\fcl_demo.exe",
        "build-r3-release\Release\fcl_demo.exe",
        "r3\build\Debug\fcl_demo.exe",
        "r3\build\Release\fcl_demo.exe"
    )
    
    $demoExe = $null
    foreach ($path in $possiblePaths) {
        $fullPath = Join-Path $script:RepoRoot $path
        if (Test-Path $fullPath) {
            $demoExe = $fullPath
            break
        }
    }
    
    if (-not $demoExe) {
        Write-Host "✗ 未找到 R3 Demo 可执行文件" -ForegroundColor Red
        Write-Host "  请先编译 R3 Demo" -ForegroundColor Yellow
        Write-Host "  尝试运行: .\tools\menu_build.ps1 -Task R3-Debug" -ForegroundColor Yellow
        Write-TestResult "R3 Demo" $false
        return
    }
    
    Write-Host "运行: $demoExe" -ForegroundColor Yellow
    & $demoExe
    
    $success = $LASTEXITCODE -eq 0
    Write-TestResult "R3 Demo" $success
    
    if ($success) {
        Write-Host "`n✓ R3 Demo 执行完成" -ForegroundColor Green
    } else {
        Write-Host "`n✗ R3 Demo 执行失败" -ForegroundColor Red
    }
}

function Test-GUIDemo {
    Write-TaskHeader "运行 GUI Demo"
    
    $guiDemoDir = Join-Path $script:ToolsDir 'gui_demo'
    
    if (-not (Test-Path $guiDemoDir)) {
        Write-Host "✗ GUI Demo 目录不存在: $guiDemoDir" -ForegroundColor Red
        Write-TestResult "GUI Demo" $false
        return
    }
    
    # 查找 GUI demo 可执行文件
    $guiExe = Get-ChildItem -Path $guiDemoDir -Filter "*.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    
    if (-not $guiExe) {
        Write-Host "✗ 未找到 GUI Demo 可执行文件" -ForegroundColor Red
        Write-Host "  GUI Demo 目录: $guiDemoDir" -ForegroundColor Yellow
        Write-TestResult "GUI Demo" $false
        return
    }
    
    Write-Host "运行: $($guiExe.FullName)" -ForegroundColor Yellow
    & $guiExe.FullName
    
    $success = $LASTEXITCODE -eq 0
    Write-TestResult "GUI Demo" $success
    
    if ($success) {
        Write-Host "`n✓ GUI Demo 执行完成" -ForegroundColor Green
    } else {
        Write-Host "`n✗ GUI Demo 执行失败" -ForegroundColor Red
    }
}

function Test-R3CTest {
    Write-TaskHeader "运行 R3 CTest (CMake 测试)"
    
    $buildDir = Join-Path $script:RepoRoot 'build\r3-demo'
    
    if (-not (Test-Path $buildDir)) {
        Write-Host "✗ 构建目录不存在: $buildDir" -ForegroundColor Red
        Write-Host "  请先编译 R3 项目" -ForegroundColor Yellow
        Write-TestResult "R3 CTest" $false
        return
    }
    
    Push-Location $buildDir
    try {
        Write-Host "运行 ctest..." -ForegroundColor Yellow
        ctest --output-on-failure -C Debug
        
        $success = $LASTEXITCODE -eq 0
        Write-TestResult "R3 CTest" $success
        
        if ($success) {
            Write-Host "`n✓ R3 CTest 通过" -ForegroundColor Green
        } else {
            Write-Host "`n✗ R3 CTest 失败" -ForegroundColor Red
        }
    }
    finally {
        Pop-Location
    }
}

function Test-FCL {
    Write-TaskHeader "运行 FCL 单元测试"
    
    $fclBuildDir = Join-Path $script:RepoRoot 'build\fcl'
    $fclSourceDir = Join-Path $script:RepoRoot 'external\fcl'
    
    if (-not (Test-Path $fclSourceDir)) {
        Write-Host "⚠ FCL 源码目录不存在，跳过测试" -ForegroundColor Yellow
        return
    }
    
    if (-not (Test-Path $fclBuildDir)) {
        New-Item -ItemType Directory -Path $fclBuildDir -Force | Out-Null
    }
    
    Push-Location $fclBuildDir
    try {
        Write-Host "配置 FCL CMake..." -ForegroundColor Yellow
        cmake $fclSourceDir `
            -DCMAKE_BUILD_TYPE=Release `
            -DBUILD_TESTING=ON `
            -DCMAKE_CXX_STANDARD=17
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "✗ FCL CMake 配置失败" -ForegroundColor Red
            Write-TestResult "FCL 测试" $false
            return
        }
        
        Write-Host "`n编译 FCL..." -ForegroundColor Yellow
        cmake --build . --config Release -j $env:NUMBER_OF_PROCESSORS
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "✗ FCL 编译失败" -ForegroundColor Red
            Write-TestResult "FCL 测试" $false
            return
        }
        
        Write-Host "`n运行 FCL 测试..." -ForegroundColor Yellow
        ctest -C Release --output-on-failure
        
        $success = $LASTEXITCODE -eq 0
        Write-TestResult "FCL 测试" $success
        
        if ($success) {
            Write-Host "`n✓ FCL 测试通过" -ForegroundColor Green
        } else {
            Write-Host "`n✗ FCL 测试失败" -ForegroundColor Red
        }
    }
    finally {
        Pop-Location
    }
}

function Test-Eigen {
    Write-TaskHeader "运行 Eigen 单元测试"
    
    $eigenBuildDir = Join-Path $script:RepoRoot 'build\eigen'
    $eigenSourceDir = Join-Path $script:RepoRoot 'external\eigen'
    
    if (-not (Test-Path $eigenSourceDir)) {
        Write-Host "⚠ Eigen 源码目录不存在，跳过测试" -ForegroundColor Yellow
        return
    }
    
    if (-not (Test-Path $eigenBuildDir)) {
        New-Item -ItemType Directory -Path $eigenBuildDir -Force | Out-Null
    }
    
    Push-Location $eigenBuildDir
    try {
        Write-Host "配置 Eigen CMake..." -ForegroundColor Yellow
        cmake $eigenSourceDir `
            -DCMAKE_BUILD_TYPE=Release `
            -DBUILD_TESTING=ON
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "✗ Eigen CMake 配置失败" -ForegroundColor Red
            Write-TestResult "Eigen 测试" $false
            return
        }
        
        Write-Host "`n编译 Eigen..." -ForegroundColor Yellow
        cmake --build . --config Release -j $env:NUMBER_OF_PROCESSORS
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "✗ Eigen 编译失败" -ForegroundColor Red
            Write-TestResult "Eigen 测试" $false
            return
        }
        
        Write-Host "`n运行 Eigen 测试..." -ForegroundColor Yellow
        ctest -C Release --output-on-failure
        
        $success = $LASTEXITCODE -eq 0
        Write-TestResult "Eigen 测试" $success
        
        if ($success) {
            Write-Host "`n✓ Eigen 测试通过" -ForegroundColor Green
        } else {
            Write-Host "`n✗ Eigen 测试失败" -ForegroundColor Red
        }
    }
    finally {
        Pop-Location
    }
}

function Test-All {
    Write-TaskHeader "运行所有测试"
    
    Test-R0Demo
    Test-R3Demo
    Test-R3CTest
    Test-FCL
    Test-Eigen
    
    Show-TestSummary
}

# 执行任务
try {
    switch ($Task) {
        'R0-Demo'    { Test-R0Demo }
        'R3-Demo'    { Test-R3Demo }
        'GUI-Demo'   { Test-GUIDemo }
        'R3-CTest'   { Test-R3CTest }
        'FCL-Test'   { Test-FCL }
        'Eigen-Test' { Test-Eigen }
        'All-Tests'  { Test-All }
    }
    
    if ($script:TotalTests -gt 0) {
        Show-TestSummary
    }
    
    # 成功时等待回车
    Write-Host ""
    Read-Host "按 Enter 继续"
}
catch {
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Red
    Write-Host "  ✗ 测试任务失败" -ForegroundColor Red
    Write-Host "============================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "错误: $_" -ForegroundColor Red
    Write-Host ""
    
    # 失败时等待回车
    Read-Host "按 Enter 返回菜单"
    
    exit 1
}
