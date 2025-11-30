<#
.SYNOPSIS
    生成文档
#>

$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$script:RepoRoot = Get-FCLRepoRoot

Write-FCLHeader "生成文档"

$docsDir = Join-Path $script:RepoRoot 'docs'

if (-not (Test-Path $docsDir)) {
    Write-Host "No docs directory found" -ForegroundColor Yellow
    exit 0
}

Write-Host "Documentation directory: $docsDir" -ForegroundColor Cyan
Write-Host ""
Write-Host "Available documentation:" -ForegroundColor Yellow

Get-ChildItem -Path $docsDir -Filter *.md | ForEach-Object {
    Write-Host "  - $($_.Name)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "To generate additional documentation, add a doc generation tool" -ForegroundColor Yellow
Write-Host "like Doxygen or Sphinx to this project." -ForegroundColor Yellow

Write-FCLSuccess "文档列表已显示"
