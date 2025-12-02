<#
.SYNOPSIS
    Restore NuGet packages for kernel driver build
    
.DESCRIPTION
    使用 NuGet CLI 将 Musa.Core, Musa.CoreLite, Musa.Veil 安装到全局缓存
    这些包将被安装到 $(USERPROFILE)\.nuget\packages\，供 MSBuild 使用
#>

$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot

Write-Host "Restoring kernel driver NuGet packages..." -ForegroundColor Cyan

# Path to packages.config
$repoRoot = (Resolve-Path (Join-Path $scriptDir '../..')).ProviderPath
$packagesConfig = Join-Path $repoRoot 'kernel\driver\msbuild\packages.config'

if (-not (Test-Path $packagesConfig)) {
    Write-Host "✗ packages.config not found at $packagesConfig" -ForegroundColor Red
    exit 1
}

# packages.config format ONLY works with nuget.exe, not with dotnet CLI
# So we must use nuget.exe for this script

# First check if nuget.exe is in PATH (system-wide installation)
$nugetExe = $null
$nugetInPath = Get-Command nuget.exe -ErrorAction SilentlyContinue
if ($nugetInPath) {
    $nugetExe = $nugetInPath.Source
    Write-Host "Using system nuget.exe from PATH: $nugetExe" -ForegroundColor Green
} else {
    # Check local nuget.exe
    $localNuget = Join-Path $scriptDir 'nuget.exe'
    if (Test-Path $localNuget) {
        $nugetExe = $localNuget
        Write-Host "Using local nuget.exe: $nugetExe" -ForegroundColor Gray
    } else {
        # Download as last resort
        Write-Host "nuget.exe not found in PATH or locally, downloading..." -ForegroundColor Yellow
        try {
            Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile $localNuget -TimeoutSec 30
            $nugetExe = $localNuget
            Write-Host "✓ nuget.exe downloaded successfully" -ForegroundColor Green
        } catch {
            throw "Failed to download nuget.exe and it's not available in PATH. Please install NuGet CLI or .NET SDK. Error: $($_.Exception.Message)"
        }
    }
}

Write-Host "Running nuget restore..."
& $nugetExe restore $packagesConfig -PackagesDirectory "$env:USERPROFILE\.nuget\packages" -NonInteractive
$exitCode = $LASTEXITCODE

if ($null -eq $exitCode) { $exitCode = 0 }

if ($exitCode -ne 0) {
    Write-Host "✗ NuGet restore failed with exit code $exitCode" -ForegroundColor Red
    exit $exitCode
}

# Verify packages were installed
$packages = @(
    @{Name='Musa.Core'; Version='0.4.1'},
    @{Name='Musa.CoreLite'; Version='1.0.3'},
    @{Name='Musa.Veil'; Version='1.5.0'}
)

Write-Host "`nVerifying installed packages:" -ForegroundColor Cyan
$allInstalled = $true

foreach ($pkg in $packages) {
    $pkgPath = Join-Path "$env:USERPROFILE\.nuget\packages" "$($pkg.Name.ToLower())\$($pkg.Version)"
    if (Test-Path $pkgPath) {
        Write-Host "  ✓ $($pkg.Name) $($pkg.Version)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $($pkg.Name) $($pkg.Version) - Not found" -ForegroundColor Red
        $allInstalled = $false
    }
}

if ($allInstalled) {
    Write-Host "`n✓ All kernel driver dependencies installed successfully" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n✗ Some packages failed to install" -ForegroundColor Red
    exit 1
}
