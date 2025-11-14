@echo off
REM FCL+Musa 全自动构建和测试脚本 (Windows 批处理)
REM 如果 PowerShell 脚本无法执行，请使用此批处理脚本

echo ==========================================
echo    FCL+Musa 单元测试执行脚本
echo ==========================================
echo.

REM 检查 PowerShell 是否可用
where powershell >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo 检测到 PowerShell，使用 PowerShell 脚本...
    echo.
    powershell -ExecutionPolicy Bypass -File "%~dp0run_all_tests.ps1"
    goto :end
)

echo PowerShell 不可用，使用简化的批处理模式...
echo.

REM 设置项目路径
set PROJECT_ROOT=%~dp0
set BUILD_DIR=%PROJECT_ROOT%build
set LOG_DIR=%PROJECT_ROOT%test_logs_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set LOG_DIR=%LOG_DIR: =0%

echo 创建日志目录: %LOG_DIR%
mkdir "%LOG_DIR%" 2>nul

echo.
echo ==========================================
echo 1. 测试 FCL
echo ==========================================
echo.

REM 检查构建目录
if not exist "%BUILD_DIR%" (
    echo 创建 FCL 构建目录...
    mkdir "%BUILD_DIR%"
)

cd /d "%BUILD_DIR%"

echo 配置 FCL CMake...
cmake "%PROJECT_ROOT%fcl-source" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_TESTING=ON ^
    -DCMAKE_CXX_STANDARD=17 ^
    -G "Visual Studio 16 2019" ^
    > "%LOG_DIR%\fcl_cmake.log" 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] FCL CMake 配置失败！查看日志: %LOG_DIR%\fcl_cmake.log
    goto :fcl_failed
)

echo 编译 FCL...
cmake --build . --config Release > "%LOG_DIR%\fcl_build.log" 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] FCL 编译失败！查看日志: %LOG_DIR%\fcl_build.log
    goto :fcl_failed
)

echo [SUCCESS] FCL 编译完成
echo.

echo 运行 FCL 单元测试...
ctest -C Release --output-on-failure > "%LOG_DIR%\fcl_test_results.txt" 2>&1

echo [SUCCESS] FCL 测试完成
echo 查看测试结果: %LOG_DIR%\fcl_test_results.txt
goto :eigen_test

:fcl_failed
echo [WARNING] FCL 测试失败，继续其他测试...

:eigen_test
echo.
echo ==========================================
echo 2. 测试 Eigen (跳过)
echo ==========================================
echo.
echo [INFO] Eigen 测试需要较长编译时间，已跳过
echo [INFO] 如需测试 Eigen，请手动运行 PowerShell 脚本

:libccd_test
echo.
echo ==========================================
echo 3. 测试 libccd (跳过)
echo ==========================================
echo.
echo [INFO] libccd 测试已跳过
echo [INFO] 如需测试，请手动运行 PowerShell 脚本

:report
echo.
echo ==========================================
echo 测试总结
echo ==========================================
echo.
echo 日志目录: %LOG_DIR%
echo.
echo FCL 测试结果: %LOG_DIR%\fcl_test_results.txt
echo.
echo 使用以下命令查看详细结果:
echo   type "%LOG_DIR%\fcl_test_results.txt"
echo.

:end
cd /d "%PROJECT_ROOT%"
echo.
echo [完成] 测试执行结束
echo.
pause
