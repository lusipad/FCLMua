<#
.SYNOPSIS
    Build FCL Demo (CLI and GUI)
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('CLI','GUI','All')]
    [string]$Target = 'All',

    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$script:ToolsDir = $PSScriptRoot
$script:RepoRoot = Split-Path -Parent $script:ToolsDir

# Import common functions
Import-Module (Join-Path $script:ToolsDir 'build\common.psm1') -Force

function Build-CLIDemoInternal {
    Write-Host ""
    Write-Host "Building CLI Demo ($Configuration)..." -ForegroundColor Cyan

    $cliDemoDir = Join-Path $script:RepoRoot 'samples\cli_demo'
    $buildDir = Join-Path $cliDemoDir 'build'

    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    }

    # Setup Visual Studio environment
    $vsDevCmd = Find-FCLVsDevCmd

    # Simple approach: use PowerShell to run cl.exe with VS environment
    Push-Location $buildDir
    try {
        # Call VsDevCmd and compile in one step
        $compileCmd = "call `"$vsDevCmd`" -arch=x64 >nul 2>&1 && cl /utf-8 /EHsc /std:c++17 /nologo /W4 `"$cliDemoDir\main.cpp`" /link /out:fcl_demo.exe"
        & cmd.exe /c $compileCmd

        if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $buildDir 'fcl_demo.exe'))) {
            throw "CLI Demo compilation failed"
        }
    }
    finally {
        Pop-Location
    }

    Write-Host "  ✓ CLI Demo compiled" -ForegroundColor Green
    Write-Host "    Output: $buildDir\fcl_demo.exe" -ForegroundColor Gray
}

function Build-GUIDemoInternal {
    Write-Host ""
    Write-Host "Building GUI Demo ($Configuration)..." -ForegroundColor Cyan

    $guiSrcDir = Join-Path $script:RepoRoot 'samples\gui_demo'
    $buildDir = Join-Path $guiSrcDir 'build'
    $distDir = Join-Path $script:RepoRoot 'dist\gui_demo'

    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    }

    if (-not (Test-Path $distDir)) {
        New-Item -ItemType Directory -Path $distDir -Force | Out-Null
    }

    # Setup Visual Studio environment
    $vsDevCmd = Find-FCLVsDevCmd

    # Verify source files exist
    $sources = Get-ChildItem -Path $guiSrcDir -Filter *.cpp
    if ($sources.Count -eq 0) {
        throw "No .cpp files found in $guiSrcDir"
    }

    Push-Location $buildDir
    try {
        # Call VsDevCmd and compile in one step
        $compileCmd = "call `"$vsDevCmd`" -arch=x64 >nul 2>&1 && cd /d `"$guiSrcDir\build`" && cl /utf-8 /EHsc /std:c++17 /nologo /W4 ..\*.cpp /link /out:fcl_demo.exe"
        & cmd.exe /c $compileCmd

        if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $buildDir 'fcl_demo.exe'))) {
            throw "GUI Demo compilation failed"
        }

        # Copy to dist
        Copy-Item (Join-Path $buildDir 'fcl_demo.exe') -Destination $distDir -Force
    }
    finally {
        Pop-Location
    }

    # Copy assets
    Write-Host "  Copying assets..." -ForegroundColor Yellow

    $assetsDir = Join-Path $script:ToolsDir 'build\assets'
    if (Test-Path $assetsDir) {
        $destAssets = Join-Path $distDir 'assets'
        if (Test-Path $destAssets) {
            Remove-Item -Path $destAssets -Recurse -Force
        }
        Copy-Item -Path $assetsDir -Destination $destAssets -Recurse -Force
    }

    $scenesDir = Join-Path $script:ToolsDir 'build\scenes'
    if (Test-Path $scenesDir) {
        $destScenes = Join-Path $distDir 'scenes'
        if (Test-Path $destScenes) {
            Remove-Item -Path $destScenes -Recurse -Force
        }
        Copy-Item -Path $scenesDir -Destination $destScenes -Recurse -Force
    }

    Write-Host "  ✓ GUI Demo built" -ForegroundColor Green
    Write-Host "    Output: $distDir\fcl_demo.exe" -ForegroundColor Gray
}

# Execute build
switch ($Target) {
    'CLI' {
        Build-CLIDemoInternal
    }
    'GUI' {
        Build-GUIDemoInternal
    }
    'All' {
        Build-CLIDemoInternal
        Build-GUIDemoInternal
    }
}

Write-Host ""
Write-Host "✓ Demo build completed successfully" -ForegroundColor Green
