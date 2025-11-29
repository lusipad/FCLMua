<#
.SYNOPSIS
    查找 Visual Studio VsDevCmd.bat 路径的通用工具

.DESCRIPTION
    使用 vswhere.exe 自动探测系统上安装的 Visual Studio 版本,
    返回 VsDevCmd.bat 的完整路径。支持 VS 2017-2022 所有版本。

.OUTPUTS
    返回 VsDevCmd.bat 的完整路径，如果未找到则返回空并退出码1
#>

[CmdletBinding()]
param()

$ErrorActionPreference = 'Continue'

function Find-VsDevCmd {
    # 1. 尝试使用 vswhere.exe（VS 2017+自带）
    $vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (Test-Path $vswherePath) {
        try {
            # 查找最新的 VS 安装（包含 C++ 工具）
            $vsPath = & $vswherePath -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -format value 2>$null

            if ($vsPath -and ($vsPath -is [string]) -and ($vsPath.Trim())) {
                $vsDevCmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
                if (Test-Path $vsDevCmd) {
                    return $vsDevCmd
                }
            }
        }
        catch {
            # vswhere 失败，继续尝试手动搜索
        }
    }

    # 2. 回退方案：手动搜索常见安装路径
    $candidates = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Enterprise\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path -PathType Leaf) {
            return $path
        }
    }

    # 未找到
    [Console]::Error.WriteLine("ERROR: Visual Studio not found. Please install Visual Studio 2019 or later with C++ workload.")
    return $null
}

# 执行查找并输出路径到 stdout
try {
    $vsDevCmdPath = Find-VsDevCmd
    if ($vsDevCmdPath) {
        Write-Output $vsDevCmdPath
        exit 0
    }
    else {
        exit 1
    }
}
catch {
    [Console]::Error.WriteLine("ERROR: $_")
    exit 1
}
