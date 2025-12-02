<#
.SYNOPSIS
    构建并运行 FCL+Musa R3 单元测试（fclmusa_r3_collision）
#>

param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).ProviderPath
$Timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$LogDir = Join-Path $RepoRoot "test_logs_r3_$Timestamp"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$LogFile = Join-Path $LogDir 'r3_collision_test.log'

$BuildDir = Join-Path $RepoRoot 'build\r3-tests'

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   FCL+Musa R3 单元测试 (碰撞/距离)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "日志输出: $LogFile"
Write-Host ""

if (Test-Path $BuildDir) {
    Write-Host "清理旧的 R3 测试构建目录..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Write-Host "[1/3] 配置 CMake..." -ForegroundColor Yellow
cmake -S $RepoRoot `
    -B $BuildDir `
    -DFCLMUSA_BUILD_USERLIB=ON `
    -DFCLMUSA_BUILD_KERNEL_LIB=OFF `
    -DFCLMUSA_BUILD_DRIVER=OFF `
    > $LogFile 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake 配置失败，查看日志: $LogFile" -ForegroundColor Red
    exit 1
}

Write-Host "[2/3] 构建 FclMusaR3Collision..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release --target FclMusaR3Collision >> $LogFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "构建失败，查看日志: $LogFile" -ForegroundColor Red
    exit 1
}

Write-Host "[3/3] 运行 ctest..." -ForegroundColor Yellow
Push-Location $BuildDir
$ctestSucceeded = $true
try {
    $ctestOutput = ctest -C Release -R fclmusa_r3_collision --output-on-failure 2>&1
    $ctestOutput | Tee-Object -FilePath $LogFile -Append | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $ctestSucceeded = $false
    }
}
finally {
    Pop-Location
}

if ($ctestSucceeded) {
    Write-Host ""
    Write-Host "R3 单元测试通过 ✅" -ForegroundColor Green
    Write-Host "详细日志: $LogFile" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "R3 单元测试失败 ❌" -ForegroundColor Red
    Write-Host "查看日志: $LogFile" -ForegroundColor Red
    exit 1
}
