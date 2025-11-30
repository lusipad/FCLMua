<#
.SYNOPSIS
    检查上游更新
#>

$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$script:RepoRoot = Get-FCLRepoRoot

Write-FCLHeader "检查上游更新"

# Check FCL upstream
Write-Host "[1/2] Checking FCL upstream..." -ForegroundColor Cyan
$fclExternal = Join-Path $script:RepoRoot 'external\fcl-source'

if (Test-Path $fclExternal) {
    Push-Location $fclExternal
    try {
        git fetch origin 2>&1 | Out-Null
        $status = git status -uno
        
        if ($status -match 'Your branch is behind') {
            Write-Host "  ⚠ Updates available" -ForegroundColor Yellow
            Write-Host "    Run 'git pull' in $fclExternal" -ForegroundColor Gray
        }
        elseif ($status -match 'Your branch is up to date') {
            Write-Host "  ✓ Up to date" -ForegroundColor Green
        }
        else {
            Write-Host "  ? Status unclear" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "  ✗ Failed to check: $_" -ForegroundColor Red
    }
    finally {
        Pop-Location
    }
}
else {
    Write-Host "  ⚠ FCL not found in external/" -ForegroundColor Yellow
}

# Check Musa.Runtime updates
Write-Host ""
Write-Host "[2/2] Checking Musa.Runtime updates..." -ForegroundColor Cyan

try {
    $response = Invoke-RestMethod 'https://api.nuget.org/v3/registration5-gz-semver2/musa.runtime/index.json' -TimeoutSec 5
    $latest = $response.items[-1].upper
    
    $versionFile = Join-Path $script:RepoRoot 'external\Musa.Runtime\Publish\.version'
    $installed = $null
    
    if (Test-Path $versionFile) {
        $installed = (Get-Content $versionFile -Raw).Trim()
    }
    
    if ($installed -eq $latest) {
        Write-Host "  ✓ Up to date: $latest" -ForegroundColor Green
    }
    else {
        Write-Host "  ⚠ Update available" -ForegroundColor Yellow
        if ($installed) {
            Write-Host "    Installed: $installed" -ForegroundColor Gray
        }
        Write-Host "    Latest: $latest" -ForegroundColor Gray
        Write-Host "    Run setup_dependencies.ps1 to update" -ForegroundColor Gray
    }
}
catch {
    Write-Host "  ✗ Failed to check: $_" -ForegroundColor Red
}

Write-FCLSuccess "上游检查完成"
