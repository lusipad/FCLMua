Import-Module "$PSScriptRoot\common.psm1" -Force
$wdk = Resolve-WdkEnvironment
Write-Host "Root: $($wdk.Root)"
Write-Host "Version: $($wdk.Version)"
Write-Host "IncludePaths: $($wdk.IncludePaths.Count)"
