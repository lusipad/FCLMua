<#
.SYNOPSIS
    测试任务执行脚本
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('R0-Demo','R3-Demo','GUI-Demo')]
    [string]$Task
)

$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$script:RepoRoot = Get-FCLRepoRoot

function Test-R0Demo {
    Write-FCLHeader "运行 R0 驱动测试"
    
    Write-Host "NOTE: R0 driver tests require driver installation" -ForegroundColor Yellow
    Write-Host "Please use manage_driver.ps1 to install the driver first" -ForegroundColor Yellow
    
    $manageScript = Join-Path $script:RepoRoot 'tools\manage_driver.ps1'
    if (Test-Path $manageScript) {
        Write-Host ""
        Write-Host "Driver management script: $manageScript" -ForegroundColor Cyan
    }
    
    Write-FCLSuccess "R0 测试信息已显示"
}

function Test-R3Demo {
    Write-FCLHeader "运行 R3 Demo"
    
    # Find the executable
    $buildDirs = @(
        'build\r3-demo\Release',
        'build\r3-demo\Debug'
    )
    
    $exePath = $null
    foreach ($dir in $buildDirs) {
        $candidate = Join-Path $script:RepoRoot "$dir\FclMusaUserDemo.exe"
        if (Test-Path $candidate) {
            $exePath = $candidate
            break
        }
    }
    
    if (-not $exePath) {
        throw "FclMusaUserDemo.exe not found. Please build R3 Demo first."
    }
    
    Write-Host "Running: $exePath" -ForegroundColor Yellow
    Write-Host ""
    
    & $exePath
    
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) {
        Write-FCLSuccess "R3 Demo 运行成功"
    } else {
        throw "R3 Demo exited with code $exitCode"
    }
}

function Test-GUIDemo {
    Write-FCLHeader "运行 GUI Demo"
    
    $distDir = Join-Path $script:RepoRoot 'dist\gui_demo'
    $exePath = Join-Path $distDir 'fcl_demo.exe'
    
    if (-not (Test-Path $exePath)) {
        throw "GUI Demo not found at $exePath. Please build GUI Demo first."
    }
    
    Write-Host "Running: $exePath" -ForegroundColor Yellow
    Write-Host ""
    
    Push-Location $distDir
    try {
        & $exePath
        
        $exitCode = $LASTEXITCODE
        if ($exitCode -eq 0) {
            Write-FCLSuccess "GUI Demo 运行成功"
        } else {
            Write-Host "GUI Demo exited with code $exitCode" -ForegroundColor Yellow
        }
    }
    finally {
        Pop-Location
    }
}

# Execute task
switch ($Task) {
    'R0-Demo'  { Test-R0Demo }
    'R3-Demo'  { Test-R3Demo }
    'GUI-Demo' { Test-GUIDemo }
}
