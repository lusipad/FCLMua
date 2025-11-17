# FCL+Musa 构建系统公共函数库
# 此文件包含所有构建脚本共享的函数，避免代码重复

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

<#
.SYNOPSIS
    查找 MSBuild.exe 的路径

.DESCRIPTION
    尝试从多个位置查找 MSBuild，按优先级：
    1. PATH 环境变量
    2. Visual Studio 2022（Enterprise/Professional/Community）
    3. Visual Studio 2019

.OUTPUTS
    String - MSBuild.exe 的完整路径

.EXAMPLE
    $msbuild = Get-MSBuildPath
#>
function Get-MSBuildPath {
    [CmdletBinding()]
    [OutputType([string])]
    param()

    # 1. 尝试从 PATH 查找
    $fromPath = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        Write-Verbose "Found MSBuild in PATH: $($fromPath.Source)"
        return $fromPath.Source
    }

    # 2. 扫描已知的 Visual Studio 安装位置
    $candidates = @(
        # Visual Studio 2022
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        # Visual Studio 2019
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        # Build Tools
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path -Path $path -PathType Leaf) {
            Write-Verbose "Found MSBuild at: $path"
            return $path
        }
    }

    # 3. 未找到，抛出错误并提供帮助信息
    throw @"
MSBuild.exe not found in any expected location.

Please install one of the following:
- Visual Studio 2022 (Community/Professional/Enterprise)
- Visual Studio 2019 (Community/Professional/Enterprise)
- Visual Studio Build Tools 2022/2019

Or add MSBuild.exe to your PATH environment variable.

Searched locations:
$($candidates -join "`n")
"@
}

<#
.SYNOPSIS
    确保 Musa.Runtime 发布配置存在

.DESCRIPTION
    检查 Musa.Runtime/Publish/Config 是否存在，如果不存在则从 NuGet 目录复制。
    这是为了兼容官方 NuGet 包的目录结构。

.PARAMETER RepoRoot
    仓库根目录路径

.EXAMPLE
    Ensure-MusaRuntimePublish -RepoRoot $repoRoot
#>
function Ensure-MusaRuntimePublish {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $publishConfig = Join-Path $RepoRoot 'external/Musa.Runtime/Publish/Config/Musa.Runtime.Config.props'

    if (Test-Path -Path $publishConfig -PathType Leaf) {
        Write-Verbose "Musa.Runtime publish config found: $publishConfig"
        return
    }

    Write-Verbose "Musa.Runtime publish config not found, checking NuGet directory..."

    # 尝试从 NuGet 目录复制
    $nugetConfigDir = Join-Path $RepoRoot 'external/Musa.Runtime/Musa.Runtime.NuGet'
    $nugetConfig = Join-Path $nugetConfigDir 'Musa.Runtime.Config.props'

    if (Test-Path -Path $nugetConfig -PathType Leaf) {
        $publishConfigDir = Split-Path -Parent $publishConfig
        if (-not (Test-Path -Path $publishConfigDir -PathType Container)) {
            New-Item -ItemType Directory -Force -Path $publishConfigDir | Out-Null
        }

        Write-Verbose "Copying Musa.Runtime config from NuGet directory..."
        Copy-Item -Path $nugetConfig -Destination $publishConfig -Force

        $nugetTargets = Join-Path $nugetConfigDir 'Musa.Runtime.Config.targets'
        if (Test-Path -Path $nugetTargets -PathType Leaf) {
            $publishTargets = Join-Path $publishConfigDir 'Musa.Runtime.Config.targets'
            Copy-Item -Path $nugetTargets -Destination $publishTargets -Force
        }
    }

    # 最终检查
    if (-not (Test-Path -Path $publishConfig -PathType Leaf)) {
        throw @"
Musa.Runtime publish config not found: $publishConfig

Please install the official Musa.Runtime package (for example via NuGet)
to external/Musa.Runtime/Publish/ instead of building it locally.

Expected file: $publishConfig
"@
    }

    Write-Verbose "Musa.Runtime publish config ready."
}

<#
.SYNOPSIS
    执行构建命令并检查错误

.DESCRIPTION
    执行命令并自动检查 $LASTEXITCODE，如果失败则抛出详细错误。

.PARAMETER ScriptBlock
    要执行的脚本块

.PARAMETER ErrorMessage
    失败时的错误消息

.EXAMPLE
    Invoke-BuildCommand { & $msbuild solution.sln } "MSBuild failed"
#>
function Invoke-BuildCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ScriptBlock]$ScriptBlock,

        [Parameter(Mandatory = $true)]
        [string]$ErrorMessage
    )

    & $ScriptBlock

    if ($LASTEXITCODE -ne 0) {
        throw "$ErrorMessage (Exit code: $LASTEXITCODE)"
    }
}

<#
.SYNOPSIS
    确保目录存在

.PARAMETER Path
    目录路径

.EXAMPLE
    Ensure-Directory "dist/output"
#>
function Ensure-Directory {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -Path $Path -PathType Container)) {
        Write-Verbose "Creating directory: $Path"
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
}

<#
.SYNOPSIS
    复制文件（如果存在）

.PARAMETER Source
    源文件路径

.PARAMETER Destination
    目标路径

.EXAMPLE
    Copy-IfExists "file.sys" "dist/"
#>
function Copy-IfExists {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination,

        [switch]$Force
    )

    if (Test-Path -Path $Source -PathType Leaf) {
        Write-Verbose "Copying $Source -> $Destination"
        if ($Force) {
            Copy-Item -Path $Source -Destination $Destination -Force
        } else {
            Copy-Item -Path $Source -Destination $Destination
        }
        return $true
    } else {
        Write-Verbose "File not found, skipping: $Source"
        return $false
    }
}

<#
.SYNOPSIS
    打印带颜色的步骤标题

.PARAMETER StepNumber
    步骤编号

.PARAMETER TotalSteps
    总步骤数

.PARAMETER Message
    消息内容

.EXAMPLE
    Write-BuildStep 1 3 "Building driver"
#>
function Write-BuildStep {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [int]$StepNumber,

        [Parameter(Mandatory = $true)]
        [int]$TotalSteps,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "[$StepNumber/$TotalSteps] $Message" -ForegroundColor Cyan
}

<#
.SYNOPSIS
    打印成功消息

.PARAMETER Message
    消息内容

.EXAMPLE
    Write-Success "Build completed"
#>
function Write-Success {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host $Message -ForegroundColor Green
}

<#
.SYNOPSIS
    打印警告消息

.PARAMETER Message
    消息内容

.EXAMPLE
    Write-BuildWarning "Certificate not found"
#>
function Write-BuildWarning {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "WARNING: $Message" -ForegroundColor Yellow
}

# 导出所有函数
Export-ModuleMember -Function @(
    'Get-MSBuildPath',
    'Ensure-MusaRuntimePublish',
    'Invoke-BuildCommand',
    'Ensure-Directory',
    'Copy-IfExists',
    'Write-BuildStep',
    'Write-Success',
    'Write-BuildWarning'
)
