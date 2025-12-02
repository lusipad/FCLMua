# FCL+Musa 构建系统公共函数库

$ErrorActionPreference = 'Stop'

function Get-FCLRepoRoot {
    $toolsDir = $PSScriptRoot
    $buildDir = Split-Path -Parent $toolsDir
    return Split-Path -Parent $buildDir
}

function Get-FCLBuildConfig {
    $configPath = Join-Path (Get-FCLRepoRoot) 'tools\build.config'
    $config = @{}
    
    if (-not (Test-Path $configPath -PathType Leaf)) {
        return $config
    }
    
    foreach ($line in Get-Content -Path $configPath -ErrorAction SilentlyContinue) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith('#')) {
            continue
        }
        
        $parts = $trimmed -split '=', 2
        if ($parts.Length -ne 2) {
            continue
        }
        
        $key = $parts[0].Trim()
        $value = $parts[1].Trim()
        if ($key) {
            $config[$key] = $value
        }
    }
    
    return $config
}

function Write-FCLHeader {
    param([string]$Title)
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""
}

function Write-FCLSuccess {
    param([string]$Message)
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "  ✓ $Message" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
}

function Write-FCLError {
    param([string]$Message)
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Red
    Write-Host "  ✗ $Message" -ForegroundColor Red
    Write-Host "============================================" -ForegroundColor Red
}

function Find-FCLMSBuild {
    $vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    
    if (Test-Path $vsWherePath) {
        $vsPath = & $vsWherePath -latest `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath `
            -format value 2>$null
        
        if ($vsPath) {
            $msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path $msbuild) {
                return $msbuild
            }
        }
    }
    
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
    )
    
    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    throw "MSBuild not found. Please install Visual Studio 2022."
}

function Find-FCLVsDevCmd {
    $vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    
    if (Test-Path $vsWherePath) {
        $vsPath = & $vsWherePath -latest `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath `
            -format value 2>$null
        
        if ($vsPath) {
            $vsDevCmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
            if (Test-Path $vsDevCmd) {
                return $vsDevCmd
            }
        }
    }
    
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
    )
    
    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    throw "Visual Studio not found. Please install Visual Studio 2022."
}

function Find-FCLWDK {
    $wdkRoot = $env:WDKContentRoot
    if (-not $wdkRoot) {
        $wdkRoot = 'C:\Program Files (x86)\Windows Kits\10'
    }
    
    if (-not (Test-Path $wdkRoot)) {
        throw "WDK not found at $wdkRoot"
    }
    
    $includePath = Join-Path $wdkRoot 'Include'
    $versions = @(
        Get-ChildItem -Path $includePath -Directory | 
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
            ForEach-Object {
                $includeVersionPath = $_.FullName
                $kmPath = Join-Path $includeVersionPath 'km'
                [PSCustomObject]@{
                    Name = $_.Name
                    Version = [version]$_.Name
                    IncludePath = $includeVersionPath
                    KmPath = $kmPath
                    HasKernelHeaders = Test-Path $kmPath
                }
            } |
            Sort-Object -Property Version -Descending
    )
    
    if ($versions.Count -eq 0) {
        throw "No WDK versions found in $includePath"
    }
    
    $versionsWithHeaders = @($versions | Where-Object { $_.HasKernelHeaders })
    
    $requestedVersion = $env:FCL_MUSA_WDK_VERSION
    if ($requestedVersion) {
        $requestedVersion = $requestedVersion.Trim()
    }
    
    if (-not $requestedVersion) {
        $config = Get-FCLBuildConfig
        if ($config.ContainsKey('WdkVersion')) {
            $value = $config['WdkVersion']
            if ($value) {
                $requestedVersion = $value.Trim()
            }
        }
    }
    
    $selected = $null
    if ($requestedVersion) {
        $selected = $versions | Where-Object { $_.Name -eq $requestedVersion } | Select-Object -First 1
        if (-not $selected) {
            $available = $versions.Name -join ', '
            throw "Requested WDK version $requestedVersion not found under $includePath. Installed versions: $available"
        }
    }
    else {
        if ($versionsWithHeaders.Count -eq 0) {
            throw "No WDK installations with kernel mode headers found in $includePath"
        }
        
        $preferredVersions = @('10.0.22621.0', '10.0.26100.0', '10.0.22000.0')
        foreach ($preferred in $preferredVersions) {
            $match = $versionsWithHeaders | Where-Object { $_.Name -eq $preferred } | Select-Object -First 1
            if ($match) {
                $selected = $match
                break
            }
        }
        
        if (-not $selected) {
            $selected = $versionsWithHeaders[0]
        }
    }
    
    $version = $selected.Name
    $kmPath = $selected.KmPath
    
    if (-not (Test-Path $kmPath)) {
        throw "WDK kernel mode headers not found at $kmPath"
    }
    
    return @{
        Root = $wdkRoot
        Version = $version
        IncludePath = $selected.IncludePath
        LibPath = Join-Path $wdkRoot "Lib\$version"
    }
}

function Setup-FCLDependencies {
    param([string]$RepoRoot)
    
    Write-Host "Setting up dependencies..." -ForegroundColor Yellow
    
    $publishDir = Join-Path $RepoRoot 'external\Musa.Runtime\Publish'
    $versionFile = Join-Path $publishDir '.version'
    
    # Check if already setup
    $latest = $null
    try {
        $response = Invoke-RestMethod 'https://api.nuget.org/v3/registration5-gz-semver2/musa.runtime/index.json' -TimeoutSec 5
        $latest = $response.items[-1].upper
        Write-Host "  Latest Musa.Runtime version: $latest" -ForegroundColor Gray
    }
    catch {
        Write-Host "  Could not fetch latest version (using cached if available)" -ForegroundColor Yellow
    }
    
    $installed = $null
    if (Test-Path $versionFile) {
        $installed = Get-Content $versionFile -Raw -ErrorAction SilentlyContinue
        $installed = $installed.Trim()
        Write-Host "  Installed Musa.Runtime version: $installed" -ForegroundColor Gray
    }
    else {
        Write-Host "  No Musa.Runtime installation found" -ForegroundColor Yellow
    }
    
    if ($installed -and $latest -and $installed -eq $latest) {
        Write-Host "  Musa.Runtime $installed is up-to-date" -ForegroundColor Green
        return
    }
    
    # Run setup_dependencies.ps1
    $setupScript = Join-Path $RepoRoot 'tools\scripts\setup_dependencies.ps1'
    if (-not (Test-Path $setupScript)) {
        throw "setup_dependencies.ps1 not found at $setupScript"
    }
    
    Write-Host "  Running setup_dependencies.ps1..." -ForegroundColor Cyan
    & pwsh -NoProfile -ExecutionPolicy Bypass -File $setupScript
    if ($LASTEXITCODE -ne 0) {
        throw "setup_dependencies.ps1 failed with exit code $LASTEXITCODE"
    }
    
    Write-Host "  Musa.Runtime setup completed" -ForegroundColor Green
    
    # Restore kernel driver NuGet packages (Musa.Core, CoreLite, Veil)
    $restoreScript = Join-Path $RepoRoot 'tools\scripts\restore_kernel_packages.ps1'
    if (Test-Path $restoreScript) {
        Write-Host "  Restoring kernel driver NuGet packages..." -ForegroundColor Cyan
        & pwsh -NoProfile -ExecutionPolicy Bypass -File $restoreScript
        if ($LASTEXITCODE -ne 0) {
            throw "restore_kernel_packages.ps1 failed with exit code $LASTEXITCODE"
        }
        Write-Host "  Kernel packages restored" -ForegroundColor Green
    } else {
        Write-Warning "restore_kernel_packages.ps1 not found, skipping kernel NuGet restore"
    }
}

function Invoke-FCLMsBuild {
    param(
        [string]$SolutionPath,
        [string]$Configuration,
        [string]$Platform = 'x64',
        [hashtable]$Properties = @{},
        [string[]]$Targets = @('Build')
    )
    
    # Find VsDevCmd.bat to setup environment
    $vsDevCmd = Find-FCLVsDevCmd
    
    # Build MSBuild arguments
    $msbuildArgs = @(
        $SolutionPath,
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/v:minimal",
        "/nologo",
        "/m"
    )
    
    foreach ($key in $Properties.Keys) {
        $msbuildArgs += "/p:$key=$($Properties[$key])"
    }
    
    if ($Targets.Count -gt 0) {
        $targetStr = $Targets -join ';'
        $msbuildArgs += "/t:$targetStr"
    }
    
    # Escape arguments for cmd.exe
    $escapedArgs = $msbuildArgs | ForEach-Object {
        if ($_ -match '\s') {
            "`"$_`""
        } else {
            $_
        }
    }
    
    # Create command that sets up VS environment and runs MSBuild
    $arch = if ($Platform -eq 'x64') { 'amd64' } else { $Platform }
    $command = "`"$vsDevCmd`" -arch=$arch && msbuild $($escapedArgs -join ' ')"
    
    Write-Host "  Running MSBuild with VS environment ($arch)..." -ForegroundColor Yellow
    
    # Execute in cmd.exe with VS environment
    & cmd.exe /c $command
    
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
}

function Invoke-FCLCMake {
    param(
        [string]$SourceDir,
        [string]$BuildDir,
        [string]$Configuration,
        [hashtable]$Options = @{}
    )
    
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    
    $args = @(
        '-S', $SourceDir,
        '-B', $BuildDir,
        '-G', 'Visual Studio 17 2022',
        '-A', 'x64'
    )
    
    foreach ($key in $Options.Keys) {
        $args += "-D$key=$($Options[$key])"
    }
    
    Write-Host "  Configuring CMake..." -ForegroundColor Yellow
    & cmake @args
    
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    
    Write-Host "  Building with CMake..." -ForegroundColor Yellow
    & cmake --build $BuildDir --config $Configuration -j $env:NUMBER_OF_PROCESSORS
    
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed"
    }
}

Export-ModuleMember -Function *
