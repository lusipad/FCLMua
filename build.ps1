<#
.SYNOPSIS
    FCL+Musa 顶层交互式构建脚本。

.DESCRIPTION
    提供常用构建/测试动作的菜单封装，内部复用 tools/ 目录下的现有脚本。
    可用于快速执行：
      1. 驱动 Debug/Release 构建（manual_build.cmd）
      2. build_all.ps1（Debug 或 Release，同时可触发 BuildRelease、BuildR3）
      3. 仅构建 R3 Demo（无驱动）
      4. CMake + ctest 的 R3 smoke test
#>
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = $scriptRoot

function Invoke-CommandChecked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$ErrorMessage = "命令执行失败。"
    )

    Write-Host ""
    Write-Host ">> $FilePath $($Arguments -join ' ')" -ForegroundColor DarkGray
    & $FilePath @Arguments
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "$ErrorMessage (ExitCode=$exitCode)"
    }
}

function Invoke-ManualBuild {
    param([ValidateSet('Debug','Release')][string]$Configuration)
    Write-Host "`n=== 构建驱动 ($Configuration) ===" -ForegroundColor Cyan
    $script = Join-Path $repoRoot 'tools\manual_build.cmd'
    Invoke-CommandChecked -FilePath $script -Arguments @($Configuration) -ErrorMessage "manual_build.cmd 失败"
    Write-Host "驱动构建完成：$Configuration" -ForegroundColor Green
}

function Invoke-BuildAll {
    param(
        [ValidateSet('Debug','Release')][string]$Configuration,
        [switch]$IncludeBuildRelease,
        [switch]$IncludeR3
    )
    Write-Host "`n=== build_all.ps1 ($Configuration) ===" -ForegroundColor Cyan
    $script = Join-Path $repoRoot 'tools\build_all.ps1'
    $args = @('-Configuration', $Configuration)
    if ($IncludeBuildRelease) { $args += '-BuildRelease' }
    if ($IncludeR3) { $args += '-BuildR3' }
    Invoke-CommandChecked -FilePath $script -Arguments $args -ErrorMessage "build_all.ps1 失败"
    Write-Host "build_all ($Configuration) 完成" -ForegroundColor Green
}

function Invoke-BuildR3Only {
    Write-Host "`n=== 仅构建 R3 Demo ===" -ForegroundColor Cyan
    $script = Join-Path $repoRoot 'tools\build_all.ps1'
    $args = @('-SkipDriver', '-SkipCLI', '-SkipGUI', '-BuildR3')
    Invoke-CommandChecked -FilePath $script -Arguments $args -ErrorMessage "R3 Demo 构建失败"
    Write-Host "R3 Demo 构建完成" -ForegroundColor Green
}

function Invoke-R3SmokeTest {
    Write-Host "`n=== 运行 R3 Smoke Test (cmake + ctest) ===" -ForegroundColor Cyan
    $buildDir = Join-Path $repoRoot 'build\r3-demo'
    $cmakeArgs = @('-S', $repoRoot, '-B', $buildDir, '-DFCLMUSA_BUILD_DRIVER=OFF', '-DFCLMUSA_BUILD_KERNEL_LIB=OFF', '-DFCLMUSA_BUILD_USERLIB=ON')
    Invoke-CommandChecked -FilePath 'cmake' -Arguments $cmakeArgs -ErrorMessage "cmake 配置失败"

    $buildArgs = @('--build', $buildDir, '--config', 'Debug', '--target', 'FclMusaR3Smoke')
    Invoke-CommandChecked -FilePath 'cmake' -Arguments $buildArgs -ErrorMessage "cmake --build 失败"

    $ctestArgs = @('--test-dir', $buildDir, '--output-on-failure', '-C', 'Debug')
    Invoke-CommandChecked -FilePath 'ctest' -Arguments $ctestArgs -ErrorMessage "ctest 执行失败"

    Write-Host "R3 smoke test 通过" -ForegroundColor Green
}

function Show-Menu {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  FCL+Musa 交互式构建面板" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "构建选项:" -ForegroundColor Yellow
    Write-Host "[1] 构建驱动 (Debug)"
    Write-Host "[2] 构建驱动 (Release)"
    Write-Host "[3] build_all (Debug)"
    Write-Host "[4] build_all (Release + BuildRelease)"
    Write-Host "[5] 仅构建 R3 Demo"
    Write-Host "[6] 运行 R3 Smoke Test (cmake + ctest)"
    Write-Host ""
    Write-Host "[0] 退出" -ForegroundColor Gray
    Write-Host ""
    Write-Host "提示: 查看 BUILD_GUIDE.md 了解更多构建选项" -ForegroundColor DarkGray
}

while ($true) {
    Show-Menu
    $choice = Read-Host "请选择操作"
    try {
        switch ($choice) {
            '1' { Invoke-ManualBuild -Configuration 'Debug' }
            '2' { Invoke-ManualBuild -Configuration 'Release' }
            '3' { Invoke-BuildAll -Configuration 'Debug' }
            '4' { Invoke-BuildAll -Configuration 'Release' -IncludeBuildRelease }
            '5' { Invoke-BuildR3Only }
            '6' { Invoke-R3SmokeTest }
            '0' { 
                Write-Host ""
                Write-Host "正在退出..." -ForegroundColor Gray
                break 
            }
            default { Write-Host "无效选项，请重新输入。" -ForegroundColor Yellow }
        }
    }
    catch {
        Write-Host "❌ 操作失败：$($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "已退出构建面板。" -ForegroundColor Gray
