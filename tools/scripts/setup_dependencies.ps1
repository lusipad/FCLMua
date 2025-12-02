<#
.SYNOPSIS
    Setup dependencies for FCL+Musa
    Downloads and extracts Musa.Runtime binaries from NuGet using dotnet/nuget cache.
    
.DESCRIPTION
    使用 NuGet 包管理器从缓存或远程源获取 Musa.Runtime。
    只有在本地不存在或版本不匹配时才会重新下载/安装。
#>

$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot

# Define paths
$repoRoot = (Resolve-Path (Join-Path $scriptDir '../..')).ProviderPath
$musaRuntimeDir = Join-Path $repoRoot 'external/Musa.Runtime'
$publishDir = Join-Path $musaRuntimeDir 'Publish'
$versionFile = Join-Path $publishDir '.version'

# Fetch latest version from NuGet API
Write-Host "Checking Musa.Runtime version..."
try {
    $latest = Invoke-RestMethod 'https://api.nuget.org/v3/registration5-gz-semver2/musa.runtime/index.json' -TimeoutSec 5
    $latestVersion = $latest.items[-1].upper
}
catch {
    Write-Host "Warning: Could not check latest version online, using cached version if available" -ForegroundColor Yellow
    $latestVersion = $null
}

# Check if already setup with correct version
$installedVersion = $null
if (Test-Path $versionFile) {
    $installedVersion = Get-Content $versionFile -ErrorAction SilentlyContinue
}

if ($installedVersion -and $installedVersion -eq $latestVersion) {
    Write-Host "Musa.Runtime $installedVersion is already installed and up-to-date."
    exit 0
}

if ($latestVersion) {
    Write-Host "Latest Version: $latestVersion"
    if ($installedVersion) {
        Write-Host "Installed Version: $installedVersion (updating...)" -ForegroundColor Yellow
    }
} else {
    Write-Host "Will attempt to restore from cache..." -ForegroundColor Yellow
}

# Create temporary project file for NuGet restore
$tempDir = Join-Path $scriptDir 'temp_nuget_restore'
if (Test-Path $tempDir) {
    Remove-Item -Path $tempDir -Recurse -Force
}
New-Item -ItemType Directory -Path $tempDir | Out-Null

# Determine version to install
$versionToInstall = if ($latestVersion) { $latestVersion } else { "0.5.1" }

# Restore package to temp directory
$packagesDir = Join-Path $tempDir 'packages'

# Check if dotnet is available, prefer it over nuget.exe
$useDotnet = $null -ne (Get-Command dotnet -ErrorAction SilentlyContinue)

if ($useDotnet) {
    Write-Host "Restoring Musa.Runtime using dotnet CLI..." -ForegroundColor Cyan
    # Create a temporary project to install the package
    $tempCsproj = Join-Path $tempDir 'temp.csproj'
    $csprojContent = @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net6.0</TargetFramework>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="Musa.Runtime" Version="$versionToInstall" />
  </ItemGroup>
</Project>
"@
    Set-Content -Path $tempCsproj -Value $csprojContent -Encoding UTF8
    dotnet restore $tempCsproj --packages $packagesDir | Out-Null
    $exitCode = $LASTEXITCODE
} else {
    Write-Host "Restoring Musa.Runtime using nuget.exe..." -ForegroundColor Cyan
    # Fallback to nuget.exe
    $packagesConfig = @"
<?xml version="1.0" encoding="utf-8"?>
<packages>
  <package id="Musa.Runtime" version="$versionToInstall" targetFramework="native" />
</packages>
"@
    $packagesConfigPath = Join-Path $tempDir 'packages.config'
    Set-Content -Path $packagesConfigPath -Value $packagesConfig -Encoding UTF8
    
    $nugetExe = Join-Path $scriptDir 'nuget.exe'
    if (Test-Path $nugetExe) {
        Write-Host "  Using existing nuget.exe" -ForegroundColor Gray
    } else {
        Write-Host "  Downloading nuget.exe..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile $nugetExe
        Write-Host "  ✓ nuget.exe downloaded successfully" -ForegroundColor Green
    }
    
    & $nugetExe restore $packagesConfigPath -PackagesDirectory $packagesDir -NonInteractive | Out-Null
    $exitCode = $LASTEXITCODE
}

if ($null -eq $exitCode) { $exitCode = 0 }

if ($exitCode -ne 0) {
    throw "NuGet restore failed with exit code $exitCode"
}

# Find the extracted package
# dotnet restore uses lowercase package names: musa.runtime/version
# nuget.exe uses mixed case: Musa.Runtime.version
if ($useDotnet) {
    $packageDir = Join-Path $packagesDir "musa.runtime\$versionToInstall"
    if (-not (Test-Path $packageDir)) {
        throw "Package not found after restore. Expected path: $packageDir"
    }
} else {
    $packageDir = Get-ChildItem -Path $packagesDir -Directory | Where-Object { $_.Name -like "Musa.Runtime.*" } | Select-Object -First 1
    if (-not $packageDir) {
        throw "Package not found after restore"
    }
    $packageDir = $packageDir.FullName
}

Write-Host "Installing to $publishDir..."
if (-not (Test-Path $publishDir)) {
    New-Item -ItemType Directory -Path $publishDir | Out-Null
}

# Copy build/native/* to Publish/
$source = Join-Path $packageDir 'build/native'
if (Test-Path $source) {
    Copy-Item -Path "$source\*" -Destination $publishDir -Recurse -Force
} else {
    throw "Package structure not as expected. Expected build/native directory at $source"
}

# Save version info
Set-Content -Path $versionFile -Value $versionToInstall -Encoding UTF8

# Cleanup temp directory
Remove-Item -Path $tempDir -Recurse -Force

Write-Host "✓ Musa.Runtime $versionToInstall installed successfully" -ForegroundColor Green
Write-Host "  Package is cached globally and will be reused on next run" -ForegroundColor Gray

