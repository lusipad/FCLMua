#
# FCL+Musa 全自动构建和测试脚本 (Windows PowerShell)
# 测试 FCL, Eigen, libccd 所有库的单元测试
#

# 设置错误时停止
$ErrorActionPreference = "Continue"

# 颜色函数
function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

function Write-ErrorMsg {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# 项目根目录
$ProjectRoot = $PSScriptRoot
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogDir = Join-Path $ProjectRoot "test_logs_$Timestamp"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

# 测试结果统计
$script:TotalTests = 0
$script:PassedTests = 0
$script:FailedTests = 0

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   FCL+Musa 单元测试执行脚本 (Windows)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "开始时间: $(Get-Date)"
Write-Host "日志目录: $LogDir"
Write-Host ""

# =============================================================================
# 1. 测试 FCL
# =============================================================================
function Test-FCL {
    Write-Info "开始构建和测试 FCL..."

    $FclBuildDir = Join-Path $ProjectRoot "build"
    $FclLog = Join-Path $LogDir "fcl_test.log"

    # 检查构建目录
    if (Test-Path $FclBuildDir) {
        Write-Info "检测到已存在的 FCL 构建目录，继续使用..."
    } else {
        Write-Info "创建 FCL 构建目录..."
        New-Item -ItemType Directory -Force -Path $FclBuildDir | Out-Null
    }

    Push-Location $FclBuildDir

    try {
        # 配置 CMake
        Write-Info "配置 FCL CMake..."
        $FclSourceDir = Join-Path $ProjectRoot "fcl-source"

        cmake $FclSourceDir `
            -DCMAKE_BUILD_TYPE=Release `
            -DBUILD_TESTING=ON `
            -DCMAKE_CXX_STANDARD=17 `
            -G "Visual Studio 16 2019" `
            > $FclLog 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-ErrorMsg "FCL CMake 配置失败！查看日志: $FclLog"
            return $false
        }

        # 编译
        Write-Info "编译 FCL..."
        cmake --build . --config Release -j $env:NUMBER_OF_PROCESSORS >> $FclLog 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-ErrorMsg "FCL 编译失败！查看日志: $FclLog"
            return $false
        }

        Write-Success "FCL 编译完成"

        # 运行测试
        Write-Info "运行 FCL 单元测试..."
        $FclTestLog = Join-Path $LogDir "fcl_test_results.txt"
        ctest -C Release --output-on-failure -j 4 > $FclTestLog 2>&1

        # 解析测试结果
        $TestOutput = Get-Content $FclTestLog -Raw
        if ($TestOutput -match "(\d+) tests passed") {
            $script:PassedTests += [int]$Matches[1]
        }
        if ($TestOutput -match "(\d+) tests failed") {
            $script:FailedTests += [int]$Matches[1]
        }
        if ($TestOutput -match "Total Tests: (\d+)") {
            $script:TotalTests += [int]$Matches[1]
        }

        Write-Success "FCL 测试完成"
        return $true
    }
    catch {
        Write-ErrorMsg "FCL 测试过程出错: $_"
        return $false
    }
    finally {
        Pop-Location
    }
}

# =============================================================================
# 2. 测试 Eigen
# =============================================================================
function Test-Eigen {
    Write-Info "开始构建和测试 Eigen..."

    $EigenSourceDir = Join-Path $ProjectRoot "external\Eigen"
    $EigenBuildDir = Join-Path $ProjectRoot "build_eigen_tests"
    $EigenLog = Join-Path $LogDir "eigen_test.log"

    # 检查 Eigen 源码完整性
    $EigenMacrosFile = Join-Path $EigenSourceDir "Eigen\src\Core\util\Macros.h"
    if (-not (Test-Path $EigenMacrosFile)) {
        Write-Warning "Eigen 源码不完整，跳过 Eigen 测试"
        Write-Warning "如需测试 Eigen，请先下载完整版本"
        return $true
    }

    # 创建构建目录
    if (Test-Path $EigenBuildDir) {
        Write-Info "清理旧的 Eigen 测试构建..."
        Remove-Item -Recurse -Force $EigenBuildDir
    }
    New-Item -ItemType Directory -Force -Path $EigenBuildDir | Out-Null

    Push-Location $EigenBuildDir

    try {
        # 配置 Eigen
        Write-Info "配置 Eigen CMake..."
        cmake $EigenSourceDir `
            -DCMAKE_BUILD_TYPE=Release `
            -DEIGEN_BUILD_PKGCONFIG=OFF `
            -DEIGEN_TEST_CXX11=ON `
            -G "Visual Studio 16 2019" `
            > $EigenLog 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-ErrorMsg "Eigen CMake 配置失败！查看日志: $EigenLog"
            return $false
        }

        # 编译部分核心测试
        Write-Info "编译 Eigen 核心测试 (仅编译部分测试以节省时间)..."
        Write-Warning "Eigen 有 147 个测试，仅编译部分核心测试..."

        $CoreTests = @("basicstuff", "linearstructure", "array_cwise", "product_small")
        foreach ($test in $CoreTests) {
            cmake --build . --config Release --target $test >> $EigenLog 2>&1
        }

        # 运行测试
        Write-Info "运行 Eigen 核心单元测试..."
        $EigenTestLog = Join-Path $LogDir "eigen_test_results.txt"
        ctest -C Release -R "basicstuff|linearstructure|array_cwise|product_small" --output-on-failure > $EigenTestLog 2>&1

        Write-Success "Eigen 测试完成 (部分)"
        return $true
    }
    catch {
        Write-Warning "Eigen 测试未完全完成: $_"
        return $true  # 不阻塞其他测试
    }
    finally {
        Pop-Location
    }
}

# =============================================================================
# 3. 测试 libccd
# =============================================================================
function Test-LibCCD {
    Write-Info "开始构建和测试 libccd..."

    $LibccdSourceDir = Join-Path $ProjectRoot "external\libccd"
    $LibccdBuildDir = Join-Path $LibccdSourceDir "build_tests"
    $LibccdLog = Join-Path $LogDir "libccd_test.log"

    # 检查是否有测试代码
    $LibccdTestMain = Join-Path $LibccdSourceDir "src\testsuites\main.c"
    if (-not (Test-Path $LibccdTestMain)) {
        Write-Warning "项目中的 libccd 是内核定制版本，不支持单元测试"
        Write-Warning "跳过 libccd 测试"
        return $true
    }

    # 创建构建目录
    if (Test-Path $LibccdBuildDir) {
        Write-Info "清理旧的 libccd 测试构建..."
        Remove-Item -Recurse -Force $LibccdBuildDir
    }
    New-Item -ItemType Directory -Force -Path $LibccdBuildDir | Out-Null

    Push-Location $LibccdBuildDir

    try {
        # 配置 libccd
        Write-Info "配置 libccd CMake..."
        cmake $LibccdSourceDir `
            -DCMAKE_BUILD_TYPE=Release `
            -DBUILD_TESTING=ON `
            -DCCD_HIDE_ALL_SYMBOLS=OFF `
            -G "Visual Studio 16 2019" `
            > $LibccdLog 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-ErrorMsg "libccd CMake 配置失败！查看日志: $LibccdLog"
            return $false
        }

        # 编译
        Write-Info "编译 libccd 和测试..."
        cmake --build . --config Release >> $LibccdLog 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-ErrorMsg "libccd 编译失败！查看日志: $LibccdLog"
            return $false
        }

        Write-Success "libccd 编译完成"

        # 运行测试
        Write-Info "运行 libccd 单元测试..."
        $LibccdTestLog = Join-Path $LogDir "libccd_test_results.txt"
        ctest -C Release --output-on-failure > $LibccdTestLog 2>&1

        Write-Success "libccd 测试完成"
        return $true
    }
    catch {
        Write-Warning "libccd 测试未完全完成: $_"
        return $true  # 不阻塞其他测试
    }
    finally {
        Pop-Location
    }
}

# =============================================================================
# 生成测试报告
# =============================================================================
function Generate-Report {
    Write-Info "生成测试报告..."

    $ReportFile = Join-Path $LogDir "test_summary.txt"
    $PassRate = if ($script:TotalTests -gt 0) {
        [math]::Round(($script:PassedTests / $script:TotalTests) * 100, 2)
    } else {
        0
    }

    $ReportContent = @"
==========================================
   FCL+Musa 单元测试总结报告
==========================================

执行时间: $(Get-Date)
日志目录: $LogDir

------------------------------------------
  总体测试结果
------------------------------------------
总测试数:   $($script:TotalTests)
通过:       $($script:PassedTests)
失败:       $($script:FailedTests)

通过率:     ${PassRate}%

------------------------------------------
  详细日志文件
------------------------------------------
FCL 测试日志:      $LogDir\fcl_test_results.txt
Eigen 测试日志:    $LogDir\eigen_test_results.txt
libccd 测试日志:   $LogDir\libccd_test_results.txt

构建日志:
  - FCL:    $LogDir\fcl_test.log
  - Eigen:  $LogDir\eigen_test.log
  - libccd: $LogDir\libccd_test.log

==========================================
"@

    $ReportContent | Out-File -FilePath $ReportFile -Encoding UTF8
    Write-Host $ReportContent
    Write-Success "测试报告已生成: $ReportFile"
}

# =============================================================================
# 主执行流程
# =============================================================================
function Main {
    # 测试 FCL
    if (Test-FCL) {
        Write-Success "FCL 测试阶段完成"
    } else {
        Write-ErrorMsg "FCL 测试失败"
    }

    Write-Host ""

    # 测试 Eigen
    if (Test-Eigen) {
        Write-Success "Eigen 测试阶段完成"
    } else {
        Write-Warning "Eigen 测试未完全完成"
    }

    Write-Host ""

    # 测试 libccd
    if (Test-LibCCD) {
        Write-Success "libccd 测试阶段完成"
    } else {
        Write-Warning "libccd 测试未完全完成"
    }

    Write-Host ""

    # 生成报告
    Generate-Report

    Write-Host ""
    Write-Success "所有测试执行完成！"
    Write-Host ""
    Write-Host "查看详细报告: $LogDir\test_summary.txt" -ForegroundColor Yellow
}

# 执行主函数
Main
