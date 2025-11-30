<#
.SYNOPSIS
    验证新菜单系统功能

.DESCRIPTION
    测试所有新菜单脚本的基本功能是否正常
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

$script:RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Definition)
$script:ToolsDir = Join-Path $script:RepoRoot 'tools'

$script:PassedTests = 0
$script:FailedTests = 0

function Test-ScriptExists {
    param(
        [string]$Name,
        [string]$Path
    )
    
    if (Test-Path $Path) {
        Write-Host "  ✓ $Name" -ForegroundColor Green
        $script:PassedTests++
        return $true
    } else {
        Write-Host "  ✗ $Name - 文件不存在: $Path" -ForegroundColor Red
        $script:FailedTests++
        return $false
    }
}

function Test-ScriptSyntax {
    param(
        [string]$Name,
        [string]$Path
    )
    
    try {
        $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content $Path -Raw), [ref]$null)
        Write-Host "  ✓ $Name - 语法正确" -ForegroundColor Green
        $script:PassedTests++
        return $true
    }
    catch {
        Write-Host "  ✗ $Name - 语法错误: $_" -ForegroundColor Red
        $script:FailedTests++
        return $false
    }
}

function Test-ScriptHelp {
    param(
        [string]$Name,
        [string]$Path
    )
    
    try {
        $help = Get-Help $Path -ErrorAction Stop
        if ($help.Synopsis) {
            Write-Host "  ✓ $Name - 帮助文档存在" -ForegroundColor Green
            $script:PassedTests++
            return $true
        }
    }
    catch {
    }
    
    Write-Host "  ⚠ $Name - 帮助文档缺失" -ForegroundColor Yellow
    return $false
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  验证新菜单系统" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 测试主脚本存在性
Write-Host "1. 检查脚本文件..." -ForegroundColor Yellow
Write-Host ""

$scripts = @{
    '主菜单' = 'menu.ps1'
    '构建脚本' = 'tools\menu_build.ps1'
    '测试脚本' = 'tools\menu_test.ps1'
    '文档脚本' = 'tools\menu_doc.ps1'
    '环境检查' = 'tools\menu_checkenv.ps1'
    '上游检查' = 'tools\menu_upstream.ps1'
}

foreach ($item in $scripts.GetEnumerator()) {
    $path = Join-Path $script:RepoRoot $item.Value
    Test-ScriptExists -Name $item.Key -Path $path
}

Write-Host ""

# 测试语法
Write-Host "2. 检查脚本语法..." -ForegroundColor Yellow
Write-Host ""

foreach ($item in $scripts.GetEnumerator()) {
    $path = Join-Path $script:RepoRoot $item.Value
    if (Test-Path $path) {
        Test-ScriptSyntax -Name $item.Key -Path $path
    }
}

Write-Host ""

# 测试帮助文档
Write-Host "3. 检查帮助文档..." -ForegroundColor Yellow
Write-Host ""

foreach ($item in $scripts.GetEnumerator()) {
    $path = Join-Path $script:RepoRoot $item.Value
    if (Test-Path $path) {
        Test-ScriptHelp -Name $item.Key -Path $path
    }
}

Write-Host ""

# 测试参数定义
Write-Host "4. 检查参数定义..." -ForegroundColor Yellow
Write-Host ""

$buildParams = @('Task', 'Platform', 'NoSign', 'Package', 'WdkVersion')
$buildScript = Join-Path $script:ToolsDir 'menu_build.ps1'

if (Test-Path $buildScript) {
    $content = Get-Content $buildScript -Raw
    $allParamsOk = $true
    
    foreach ($param in $buildParams) {
        if ($content -match "\[\w+\]\s*\`$$param\b") {
            Write-Host "  ✓ menu_build.ps1 参数: $param" -ForegroundColor Green
            $script:PassedTests++
        } else {
            Write-Host "  ✗ menu_build.ps1 参数缺失: $param" -ForegroundColor Red
            $script:FailedTests++
            $allParamsOk = $false
        }
    }
}

Write-Host ""

# 检查任务定义
Write-Host "5. 检查任务定义..." -ForegroundColor Yellow
Write-Host ""

$buildTasks = @('R0-Debug', 'R0-Release', 'R3-Debug', 'R3-Release', 'CLI-Demo', 'GUI-Demo', 'All')
$buildScript = Join-Path $script:ToolsDir 'menu_build.ps1'

if (Test-Path $buildScript) {
    $content = Get-Content $buildScript -Raw
    
    foreach ($task in $buildTasks) {
        if ($content -match "'$task'") {
            Write-Host "  ✓ Build 任务: $task" -ForegroundColor Green
            $script:PassedTests++
        } else {
            Write-Host "  ✗ Build 任务缺失: $task" -ForegroundColor Red
            $script:FailedTests++
        }
    }
}

Write-Host ""

$testTasks = @('R0-Demo', 'R3-Demo', 'GUI-Demo', 'R3-CTest', 'FCL-Test', 'Eigen-Test', 'All-Tests')
$testScript = Join-Path $script:ToolsDir 'menu_test.ps1'

if (Test-Path $testScript) {
    $content = Get-Content $testScript -Raw
    
    foreach ($task in $testTasks) {
        if ($content -match "'$task'") {
            Write-Host "  ✓ Test 任务: $task" -ForegroundColor Green
            $script:PassedTests++
        } else {
            Write-Host "  ✗ Test 任务缺失: $task" -ForegroundColor Red
            $script:FailedTests++
        }
    }
}

Write-Host ""

# 检查依赖的工具脚本
Write-Host "6. 检查依赖工具..." -ForegroundColor Yellow
Write-Host ""

$requiredTools = @{
    'manual_build.cmd' = 'tools\manual_build.cmd'
    'sign_driver.ps1' = 'tools\sign_driver.ps1'
    'setup_dependencies.ps1' = 'tools\setup_dependencies.ps1'
    'fcl-self-test.ps1' = 'tools\fcl-self-test.ps1'
    'verify_upstream.ps1' = 'tools\verify_upstream.ps1'
    'common.psm1' = 'tools\common.psm1'
}

foreach ($item in $requiredTools.GetEnumerator()) {
    $path = Join-Path $script:RepoRoot $item.Value
    Test-ScriptExists -Name $item.Key -Path $path
}

Write-Host ""

# 检查文档
Write-Host "7. 检查文档..." -ForegroundColor Yellow
Write-Host ""

$docs = @{
    '使用指南' = 'MENU_GUIDE.md'
    '快速参考' = 'MENU_REFERENCE.md'
    '设计说明' = 'MENU_DESIGN.md'
    '迁移总结' = 'MIGRATION_SUMMARY.md'
}

foreach ($item in $docs.GetEnumerator()) {
    $path = Join-Path $script:RepoRoot $item.Value
    Test-ScriptExists -Name $item.Key -Path $path
}

Write-Host ""

# 总结
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  验证结果" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  通过: $script:PassedTests" -ForegroundColor Green
Write-Host "  失败: $script:FailedTests" -ForegroundColor Red
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if ($script:FailedTests -eq 0) {
    Write-Host "✓ 所有检查通过！新菜单系统已准备就绪。" -ForegroundColor Green
    Write-Host ""
    Write-Host "下一步:" -ForegroundColor Cyan
    Write-Host "  1. 运行 .\menu.ps1 测试交互式菜单" -ForegroundColor Gray
    Write-Host "  2. 运行 .\tools\cleanup_old_scripts.ps1 -DryRun 预览清理" -ForegroundColor Gray
    Write-Host "  3. 运行 .\tools\cleanup_old_scripts.ps1 执行清理" -ForegroundColor Gray
    exit 0
} else {
    Write-Host "✗ 发现 $script:FailedTests 个问题，请修复后重试。" -ForegroundColor Red
    exit 1
}
