# FCL+Musa 构建系统公共函数库
# 此文件包含所有构建脚本共享的函数，避免代码重复

# Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

<#
.SYNOPSIS
    查找 MSBuild.exe 的路径
#>
function Get-MSBuildPath {
    [CmdletBinding()]
    [OutputType([string])]
    param()

    # 2. 扫描已知的 Visual Studio 安装位置
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($path in $candidates) {
        # Write-Host "Checking $path"
        if (Test-Path -Path $path -PathType Leaf) {
            Write-Verbose "Found MSBuild at: $path"
            return $path
        }
    }

    throw "MSBuild not found."
}

<#
.SYNOPSIS
    确保 Musa.Runtime 发布配置存在
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

    if (-not (Test-Path -Path $publishConfig -PathType Leaf)) {
        throw "Musa.Runtime publish config not found at $publishConfig. Please populate external/Musa.Runtime/Publish."
    }

    Write-Verbose "Musa.Runtime publish config ready."
}

<#
.SYNOPSIS
    执行构建命令并检查错误
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
#>
function Write-BuildWarning {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "WARNING: $Message" -ForegroundColor Yellow
}

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
