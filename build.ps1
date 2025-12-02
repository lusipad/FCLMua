<#
.SYNOPSIS
    FCL+Musa 交互式构建菜单

.DESCRIPTION
    简洁的交互式构建系统,提供编译、测试、文档等功能
#>

param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = $PSScriptRoot
$script:BuildDir = Join-Path $script:RepoRoot 'tools\build'

# 导入构建模块
Import-Module (Join-Path $script:BuildDir 'common.psm1') -Force

# 自动应用 FCL kernel 模式补丁，确保 R0/R3 构建一致
try {
    & (Join-Path $script:RepoRoot 'tools\scripts\apply_fcl_patch.ps1') -Quiet
} catch {
    Write-Warning "检测/应用 FCL Kernel 模式补丁失败：$($_.Exception.Message)"
}

function Show-MainMenu {
    Clear-Host
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "     FCL+Musa 构建系统" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  1. Build         - 编译项目"
    Write-Host "  2. Test          - 运行测试"
    Write-Host "  3. Doc           - 生成文档"
    Write-Host "  4. Check Env     - 检查环境"
    Write-Host "  5. Check Upstream - 检查上游更新"
    Write-Host "  6. FCL Patch      - 管理 external/fcl-source 补丁"
    Write-Host ""
    Write-Host "  0. Exit          - 退出"
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""
}

function Show-BuildMenu {
    Clear-Host
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host "     构建菜单" -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  1. R0 Debug        - 编译 R0 驱动 (Debug + 签名)"
    Write-Host "  2. R0 Release      - 编译 R0 驱动 (Release + 签名)"
    Write-Host "  3. R3 Lib Debug    - 编译 R3 用户态库 (Debug)"
    Write-Host "  4. R3 Lib Release  - 编译 R3 用户态库 (Release)"
    Write-Host "  5. R3 Demo Debug   - 编译 R3 Demo (Debug)"
    Write-Host "  6. R3 Demo Release - 编译 R3 Demo (Release)"
    Write-Host "  7. CLI Demo        - 编译 CLI Demo"
    Write-Host "  8. GUI Demo        - 编译 GUI Demo"
    Write-Host "  9. All             - 编译所有项目"
    Write-Host ""
    Write-Host "  0. Back            - 返回主菜单"
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ""
}

function Show-TestMenu {
    Clear-Host
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Yellow
    Write-Host "     测试菜单" -ForegroundColor Yellow
    Write-Host "============================================" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  1. R0 Demo       - 运行 R0 驱动测试"
    Write-Host "  2. R3 Demo       - 运行 R3 Demo"
    Write-Host "  3. GUI Demo      - 运行 GUI Demo"
    Write-Host "  4. R3 Unit Tests - 构建并运行 R3 单元测试"
    Write-Host "  5. All Tests     - 运行所有测试（完整测试套件）"
    Write-Host ""
    Write-Host "  0. Back          - 返回主菜单"
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Yellow
    Write-Host ""
}

function Show-PatchMenu {
    Clear-Host
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Magenta
    Write-Host "     FCL Patch 工具" -ForegroundColor Magenta
    Write-Host "============================================" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "  1. 应用补丁 (apply_fcl_patch.ps1)"
    Write-Host "  2. 恢复到上游版本 (git reset --hard + clean)"
    Write-Host ""
    Write-Host "  0. Back          - 返回主菜单"
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Magenta
    Write-Host ""
}

function Wait-ForEnter {
    param([string]$Message = "按 Enter 返回菜单")
    Write-Host ""
    Write-Host $Message -ForegroundColor Gray
    while ($true) {
        $key = [Console]::ReadKey($true)
        if ($key.Key -eq 'Enter') {
            break
        }
    }
}

function Invoke-BuildTask {
    param([string]$Task)
    
    try {
        & (Join-Path $script:BuildDir 'build-tasks.ps1') -Task $Task
        if ($LASTEXITCODE -ne 0) {
            throw "Build task failed with exit code $LASTEXITCODE"
        }
    }
    catch {
        Write-Host ""
        Write-Host "============================================" -ForegroundColor Red
        Write-Host "  ✗ 构建任务失败" -ForegroundColor Red
        Write-Host "============================================" -ForegroundColor Red
        Write-Host ""
        Write-Host "错误: $_" -ForegroundColor Red
    }
    finally {
        Wait-ForEnter
    }
}

function Invoke-TestTask {
    param([string]$Task)
    
    try {
        & (Join-Path $script:BuildDir 'test-tasks.ps1') -Task $Task
        if ($LASTEXITCODE -ne 0) {
            throw "Test task failed with exit code $LASTEXITCODE"
        }
    }
    catch {
        Write-Host ""
        Write-Host "============================================" -ForegroundColor Red
        Write-Host "  ✗ 测试任务失败" -ForegroundColor Red
        Write-Host "============================================" -ForegroundColor Red
        Write-Host ""
        Write-Host "错误: $_" -ForegroundColor Red
    }
    finally {
        Wait-ForEnter
    }
}

function Invoke-DocTask {
    try {
        & (Join-Path $script:BuildDir 'doc-tasks.ps1')
    }
    catch {
        Write-Host ""
        Write-Host "错误: $_" -ForegroundColor Red
    }
    finally {
        Wait-ForEnter "按 Enter 继续"
    }
}

function Invoke-CheckEnv {
    try {
        & (Join-Path $script:BuildDir 'check-env.ps1')
    }
    catch {
        Write-Host ""
        Write-Host "错误: $_" -ForegroundColor Red
    }
    finally {
        Wait-ForEnter "按 Enter 继续"
    }
}

function Invoke-CheckUpstream {
    try {
        & (Join-Path $script:BuildDir 'check-upstream.ps1')
    }
    catch {
        Write-Host ""
        Write-Host "错误: $_" -ForegroundColor Red
    }
    finally {
        Wait-ForEnter "按 Enter 继续"
    }
}

function Invoke-FclPatchAction {
    param(
        [ValidateSet('Apply','Restore')]
        [string]$Action
    )

    try {
        $patchScript = Join-Path $script:RepoRoot 'tools\scripts\apply_fcl_patch.ps1'
        $arguments = @()
        if ($Action -eq 'Restore') {
            $arguments += '-Restore'
        }
        & $patchScript @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "apply_fcl_patch.ps1 执行失败 (exit $LASTEXITCODE)"
        }
    }
    catch {
        Write-Host ""
        Write-Host "FCL 补丁操作失败: $_" -ForegroundColor Red
    }
    finally {
        Wait-ForEnter
    }
}

# 主循环
while ($true) {
    Show-MainMenu
    $choice = Read-Host "请选择"
    
    switch ($choice) {
        '1' {
            # Build 子菜单
            while ($true) {
                Show-BuildMenu
                $buildChoice = Read-Host "请选择"
                
                switch ($buildChoice) {
                    '1' { Invoke-BuildTask 'R0-Debug' }
                    '2' { Invoke-BuildTask 'R0-Release' }
                    '3' { Invoke-BuildTask 'R3-Lib-Debug' }
                    '4' { Invoke-BuildTask 'R3-Lib-Release' }
                    '5' { Invoke-BuildTask 'R3-Demo-Debug' }
                    '6' { Invoke-BuildTask 'R3-Demo-Release' }
                    '7' { Invoke-BuildTask 'CLI-Demo' }
                    '8' { Invoke-BuildTask 'GUI-Demo' }
                    '9' { Invoke-BuildTask 'All' }
                    '0' { break }
                    default { 
                        Write-Host "无效选择" -ForegroundColor Red
                        Start-Sleep -Seconds 1
                    }
                }
                
                if ($buildChoice -eq '0') { break }
            }
        }
        '2' {
            # Test 子菜单
            while ($true) {
                Show-TestMenu
                $testChoice = Read-Host "请选择"
                
                    switch ($testChoice) {
                        '1' { Invoke-TestTask 'R0-Demo' }
                        '2' { Invoke-TestTask 'R3-Demo' }
                        '3' { Invoke-TestTask 'GUI-Demo' }
                        '4' {
                            try {
                                & (Join-Path $script:RepoRoot 'tools\scripts\run_r3_tests.ps1')
                            }
                            catch {
                                Write-Host ""
                                Write-Host "错误: $_" -ForegroundColor Red
                            }
                            finally {
                                Wait-ForEnter
                            }
                        }
                        '5' { 
                            try {
                                & (Join-Path $script:RepoRoot 'tools\scripts\run_all_tests.ps1')
                            }
                            catch {
                                Write-Host ""
                            Write-Host "错误: $_" -ForegroundColor Red
                        }
                        finally {
                            Wait-ForEnter
                        }
                    }
                    '0' { break }
                    default { 
                        Write-Host "无效选择" -ForegroundColor Red
                        Start-Sleep -Seconds 1
                    }
                }
                
                if ($testChoice -eq '0') { break }
            }
        }
        '3' { Invoke-DocTask }
        '4' { Invoke-CheckEnv }
        '5' { Invoke-CheckUpstream }
        '6' {
            while ($true) {
                Show-PatchMenu
                $patchChoice = Read-Host "请选择"
                switch ($patchChoice) {
                    '1' { Invoke-FclPatchAction -Action 'Apply' }
                    '2' { Invoke-FclPatchAction -Action 'Restore' }
                    '0' { break }
                    default {
                        Write-Host "无效选择" -ForegroundColor Red
                        Start-Sleep -Seconds 1
                    }
                }
                if ($patchChoice -eq '0') { break }
            }
        }
        '0' {
            Write-Host ""
            Write-Host "再见！" -ForegroundColor Cyan
            exit 0
        }
        default {
            Write-Host "无效选择" -ForegroundColor Red
            Start-Sleep -Seconds 1
        }
    }
}
