<#
.SYNOPSIS
    Setup dependencies for FCL+Musa
    Downloads and extracts Musa.Runtime binaries from NuGet.
#>

$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot

# Define paths
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..')).ProviderPath
$musaRuntimeDir = Join-Path $repoRoot 'external/Musa.Runtime'
$publishDir = Join-Path $musaRuntimeDir 'Publish'
$packageZip = Join-Path $scriptDir 'Musa.Runtime.zip'
$packageExtractDir = Join-Path $scriptDir 'Musa.Runtime.Package'

# Check if already setup
if (Test-Path -Path (Join-Path $publishDir 'Library/x64/Release/Musa.Runtime.lib')) {
    Write-Host "Dependencies already setup."
    exit 0
}

Write-Host "Downloading Musa.Runtime from NuGet..."

# Fetch latest version
$latest = Invoke-RestMethod 'https://api.nuget.org/v3/registration5-gz-semver2/musa.runtime/index.json'
$version = $latest.items[-1].upper
Write-Host "Latest Version: $version"

$url = "https://www.nuget.org/api/v2/package/Musa.Runtime/$version"
Invoke-WebRequest -Uri $url -OutFile $packageZip

Write-Host "Extracting..."
Expand-Archive -Path $packageZip -DestinationPath $packageExtractDir -Force

Write-Host "Installing to $publishDir..."
if (-not (Test-Path $publishDir)) {
    New-Item -ItemType Directory -Path $publishDir | Out-Null
}

# Copy build/native/* to Publish/
# NuGet structure: build/native/include, build/native/lib, etc. 
# But in the previous turn I saw the structure was directly in build/native matching Publish
# Let's be careful with robocopy/copy behavior.
# The structure in package: build/native/Config, Include, Library
# The structure needed in Publish: Config, Include, Library

$source = Join-Path $packageExtractDir 'build/native'
Copy-Item -Path "$source\*" -Destination $publishDir -Recurse -Force

# Cleanup
Remove-Item -Path $packageZip -Force
Remove-Item -Path $packageExtractDir -Recurse -Force

Write-Host "Dependencies setup complete."
