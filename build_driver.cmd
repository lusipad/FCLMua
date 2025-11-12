@echo off
REM FCL+Musa Driver Build Script
REM 此脚本在正确的 Visual Studio 环境中构建驱动

echo ========================================
echo FCL+Musa Driver Build Script
echo ========================================
echo.

REM 设置 Visual Studio 2022 环境
echo [1/3] 初始化 Visual Studio 2022 环境...
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64

if errorlevel 1 (
    echo 错误: 无法初始化 Visual Studio 环境
    exit /b 1
)

echo.
echo [2/3] 清理之前的构建...
cd /d "%~dp0kernel\FclMusaDriver"
msbuild FclMusaDriver.sln /t:Clean /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo

echo.
echo [3/3] 构建驱动 (Debug x64)...
msbuild FclMusaDriver.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /v:normal /nologo

if errorlevel 1 (
    echo.
    echo ========================================
    echo 构建失败！请检查错误信息。
    echo ========================================
    exit /b 1
)

echo.
echo ========================================
echo 构建成功！
echo ========================================
echo.
echo 输出目录: %~dp0kernel\FclMusaDriver\out\x64\Debug\
echo.

REM 列出生成的文件
echo 生成的文件:
dir /b "%~dp0kernel\FclMusaDriver\out\x64\Debug\FclMusaDriver.*" 2>nul

echo.
pause
