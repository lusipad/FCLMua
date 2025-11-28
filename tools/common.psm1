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

<#
.SYNOPSIS
    自动探测 WDK 根目录与版本，返回可用于包含/库/工具查找的路径
#>
function Resolve-WdkEnvironment {
    [CmdletBinding()]
    [OutputType([pscustomobject])]
    param(
        [string]$RequestedVersion = $null,
        [string]$RequestedRoot = $null
    )

    $candidateRoots = @()
    if ($RequestedRoot) { $candidateRoots += $RequestedRoot }
    if ($env:FCL_MUSA_WDK_ROOT) { $candidateRoots += $env:FCL_MUSA_WDK_ROOT }
    if ($env:WDKContentRoot) { $candidateRoots += $env:WDKContentRoot }
    $candidateRoots += "${env:ProgramFiles(x86)}\Windows Kits\10"
    $candidateRoots = $candidateRoots | Where-Object { $_ } | Select-Object -Unique

    foreach ($root in $candidateRoots) {
        if (-not (Test-Path -Path $root -PathType Container)) { continue }
        $includeRoot = Join-Path $root 'Include'
        if (-not (Test-Path -Path $includeRoot -PathType Container)) { continue }

        $resolvedVersion = $null
        if ($RequestedVersion) {
            $headerCandidate = Join-Path $includeRoot (Join-Path $RequestedVersion 'km\ntddk.h')
            if (Test-Path -Path $headerCandidate -PathType Leaf) {
                $resolvedVersion = $RequestedVersion
            } else {
                continue
            }
        } else {
            $versions = Get-ChildItem -Path $includeRoot -Directory -ErrorAction SilentlyContinue |
                Where-Object { Test-Path (Join-Path $_.FullName 'km\ntddk.h') } |
                Select-Object -ExpandProperty Name
            if (-not $versions -or $versions.Count -eq 0) { continue }

            $versionInfos = foreach ($name in $versions) {
                $parsed = $null
                $ok = [System.Version]::TryParse($name, [ref]$parsed)
                [pscustomobject]@{
                    Name = $name
                    Parsed = if ($ok) { $parsed } else { $null }
                }
            }

            $ordered = $versionInfos | Sort-Object -Property @{Expression = { if ($_.Parsed) { $_.Parsed } else { [System.Version]::new(0, 0) } }; Descending = $true}
            $resolvedVersion = $ordered[0].Name
        }

        if ($resolvedVersion) {
            $includeBase = Join-Path $includeRoot $resolvedVersion
            $libBase = Join-Path (Join-Path $root 'Lib') $resolvedVersion
            $binVersioned = Join-Path (Join-Path $root 'bin') $resolvedVersion

            $includePaths = @(
                Join-Path $includeBase 'km'
                Join-Path $includeBase 'shared'
                Join-Path $includeBase 'ucrt'
                Join-Path $includeBase 'um'
            ) | Where-Object { Test-Path $_ }

            $libPaths = @(
                Join-Path $libBase 'km\x64'
                Join-Path $libBase 'um\x64'
            ) | Where-Object { Test-Path $_ }

            $binPaths = @(
                Join-Path $binVersioned 'x64'
                Join-Path $root 'bin\x64'
            ) | Where-Object { Test-Path $_ }

            return [pscustomobject]@{
                Root = (Resolve-Path $root).ProviderPath
                Version = $resolvedVersion
                IncludePaths = $includePaths
                LibPaths = $libPaths
                BinPaths = $binPaths
            }
        }
    }

    if ($RequestedVersion) {
        throw "Requested WDK version '$RequestedVersion' not found in any known root. Please install the WDK or set FCL_MUSA_WDK_ROOT."
    } else {
        throw "Unable to locate a suitable WDK installation. Please install the Windows Driver Kit or set FCL_MUSA_WDK_ROOT/FCL_MUSA_WDK_VERSION."
    }
}

Export-ModuleMember -Function @(
    'Get-MSBuildPath',
    'Ensure-MusaRuntimePublish',
    'Invoke-BuildCommand',
    'Ensure-Directory',
    'Copy-IfExists',
    'Write-BuildStep',
    'Write-Success',
    'Write-BuildWarning',
    'Resolve-WdkEnvironment'
)
