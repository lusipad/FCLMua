<#
.SYNOPSIS
    FCL+Musa 构建系统主菜单

.DESCRIPTION
    交互式主菜单，提供构建、测试、文档等功能的入口
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = $PSScriptRoot
$script:ToolsDir = Join-Path $script:RepoRoot 'tools'

function Show-MainMenu {
    Clear-Host
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "     FCL+Musa 构建系统主菜单" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  1. Build        - 编译项目"
    Write-Host "  2. Test         - 运行测试"
    Write-Host "  3. Doc          - 生成文档"
    Write-Host "  4. Check Env    - 检查环境"
    Write-Host "  5. Check Upstream - 检查上游更新"
    Write-Host ""
    Write-Host "  0. Exit         - 退出"
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
    Write-Host "  1. R0 Debug     - 编译 R0 驱动 (Debug + 签名)"
    Write-Host "  2. R0 Release   - 编译 R0 驱动 (Release + 签名)"
    Write-Host "  3. R3 Debug     - 编译 R3 Demo (Debug)"
    Write-Host "  4. R3 Release   - 编译 R3 Demo (Release)"
    Write-Host "  5. CLI Demo     - 编译 CLI Demo"
    Write-Host "  6. GUI Demo     - 编译 GUI Demo"
    Write-Host "  7. All          - 编译所有项目"
    Write-Host ""
    Write-Host "  0. Back         - 返回主菜单"
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
    Write-Host "  1. R0 Demo      - 运行 R0 驱动测试"
    Write-Host "  2. R3 Demo      - 运行 R3 Demo"
    Write-Host "  3. GUI Demo     - 运行 GUI Demo"
    Write-Host "  4. R3 CTest     - 运行 R3 CMake 测试"
    Write-Host "  5. FCL Test     - 运行 FCL 单元测试"
    Write-Host "  6. Eigen Test   - 运行 Eigen 单元测试"
    Write-Host "  7. All Tests    - 运行所有测试"
    Write-Host ""
    Write-Host "  0. Back         - 返回主菜单"
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Yellow
    Write-Host ""
}

function Invoke-BuildTask {
    param([string]$Task)
    
    $buildScript = Join-Path $script:ToolsDir 'menu_build.ps1'
    & $buildScript -Task $Task
    # 不需要额外的暂停，子脚本已经处理
}

function Invoke-TestTask {
    param([string]$Task)
    
    $testScript = Join-Path $script:ToolsDir 'menu_test.ps1'
    & $testScript -Task $Task
    # 不需要额外的暂停，子脚本已经处理
}

function Invoke-DocTask {
    $docScript = Join-Path $script:ToolsDir 'menu_doc.ps1'
    & $docScript
    
    Write-Host ""
    Write-Host "按任意键继续..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
}

function Invoke-CheckEnvTask {
    $checkScript = Join-Path $script:ToolsDir 'menu_checkenv.ps1'
    & $checkScript
    
    Write-Host ""
    Write-Host "按任意键继续..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
}

function Invoke-CheckUpstreamTask {
    $upstreamScript = Join-Path $script:ToolsDir 'menu_upstream.ps1'
    & $upstreamScript
    
    Write-Host ""
    Write-Host "按任意键继续..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
}

# 主循环
while ($true) {
    Show-MainMenu
    $choice = Read-Host "请选择"
    
    try {
        switch ($choice) {
            '1' {
                # Build 子菜单
                while ($true) {
                    Show-BuildMenu
                    $buildChoice = Read-Host "请选择"
                    
                    switch ($buildChoice) {
                        '1' { Invoke-BuildTask 'R0-Debug' }
                        '2' { Invoke-BuildTask 'R0-Release' }
                        '3' { Invoke-BuildTask 'R3-Debug' }
                        '4' { Invoke-BuildTask 'R3-Release' }
                        '5' { Invoke-BuildTask 'CLI-Demo' }
                        '6' { Invoke-BuildTask 'GUI-Demo' }
                        '7' { Invoke-BuildTask 'All' }
                        '0' { break }
                        default { 
                            Write-Host "无效选择，请重试" -ForegroundColor Red
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
                        '4' { Invoke-TestTask 'R3-CTest' }
                        '5' { Invoke-TestTask 'FCL-Test' }
                        '6' { Invoke-TestTask 'Eigen-Test' }
                        '7' { Invoke-TestTask 'All-Tests' }
                        '0' { break }
                        default { 
                            Write-Host "无效选择，请重试" -ForegroundColor Red
                            Start-Sleep -Seconds 1
                        }
                    }
                    
                    if ($testChoice -eq '0') { break }
                }
            }
            '3' {
                Invoke-DocTask
            }
            '4' {
                Invoke-CheckEnvTask
            }
            '5' {
                Invoke-CheckUpstreamTask
            }
            '0' {
                Write-Host ""
                Write-Host "再见！" -ForegroundColor Cyan
                exit 0
            }
            default {
                Write-Host "无效选择，请重试" -ForegroundColor Red
                Start-Sleep -Seconds 1
            }
        }
    }
    catch {
        Write-Host ""
        Write-Host "错误: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "按任意键继续..." -ForegroundColor Gray
        $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
    }
}
