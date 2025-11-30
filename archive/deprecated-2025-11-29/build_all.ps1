<#
.SYNOPSIS
    FCL+Musa 缁熶竴鏋勫缓鑴氭湰

.DESCRIPTION
    缁熶竴鐨勬瀯寤鸿剼鏈紝鏀寔鎵€鏈夋瀯寤哄満鏅細
    - 浠呮瀯寤洪┍鍔?
    - 鏋勫缓椹卞姩骞剁鍚?
    - 鏋勫缓鎵€鏈夌粍浠讹紙椹卞姩 + CLI Demo + GUI Demo锛?
    - 鎵撳寘鍙戝竷鍖?

    鏇夸唬浜嗕箣鍓嶇殑涓変釜鑴氭湰锛?
    - build_all.ps1锛堟棫锛?
    - build_and_sign_driver.ps1
    - build_all_and_sign.ps1锛堟棫锛?

.PARAMETER Configuration
    鏋勫缓閰嶇疆锛欴ebug 鎴?Release锛堥粯璁わ細Debug锛?

.PARAMETER Platform
    鐩爣骞冲彴锛歺64锛堥粯璁わ細x64锛?

.PARAMETER Sign
    鏄惁绛惧悕椹卞姩锛堥粯璁わ細鍚︼級

.PARAMETER DriverOnly
    浠呮瀯寤洪┍鍔紝涓嶆瀯寤?Demo

.PARAMETER SkipDriver
    璺宠繃椹卞姩鏋勫缓锛屼粎鏋勫缓 Demo

.PARAMETER SkipCLI
    璺宠繃 CLI Demo 鏋勫缓

.PARAMETER SkipGUI
    璺宠繃 GUI Demo 鏋勫缓

.PARAMETER Package
    鎵撳寘鎵€鏈変骇鐗╁埌 dist/bundle/

.PARAMETER BuildRelease
    浠庣姝ｅ紡鎵嶈鍑烘繁鍙湴杩涜 Release 榛樿閰嶇疆椹卞姩鏋勫缓锛岀洿鎺ュ皢鏋勫缓瑙ｉ噴缁欏埌 tools/manual_build.cmd Release

.PARAMETER BuildR3
    鏋勫缓浣跨敤 FclMusa::CoreUser 鐨?R3 demo锛岀敱姝ゅ強纭姝ｅ紡鐨勯偅绉嶆敞鍐岀敤鎴风姸鎬佺敤

.PARAMETER WdkVersion
    鎸囧畾 WDK 鐗堟湰锛堜緥濡傦細10.0.22621.0, 10.0.26100.0锛?
    濡傛灉涓嶆寚瀹氾紝灏嗚嚜鍔ㄦ娴嬬郴缁熶笂鍙敤鐨?WDK 鐗堟湰

.EXAMPLE
    .\build_all.ps1
    鏋勫缓鎵€鏈夌粍浠讹紙Debug锛屼笉绛惧悕锛?

.EXAMPLE
    .\build_all.ps1 -Configuration Release -Sign
    鏋勫缓鎵€鏈夌粍浠讹紙Release锛夊苟绛惧悕椹卞姩

.EXAMPLE
    .\build_all.ps1 -DriverOnly -Sign
    浠呮瀯寤哄苟绛惧悕椹卞姩

.EXAMPLE
    .\build_all.ps1 -Configuration Release -Sign -Package
    鏋勫缓鎵€鏈夌粍浠躲€佺鍚嶉┍鍔ㄥ苟鎵撳寘锛堝畬鏁村彂甯冩祦绋嬶級

.EXAMPLE
    .\build_all.ps1 -SkipGUI
    鏋勫缓椹卞姩鍜?CLI Demo锛岃烦杩?GUI Demo

.EXAMPLE
    .\build_all.ps1 -WdkVersion 10.0.26100.0
    浣跨敤鎸囧畾鐨?WDK 鐗堟湰杩涜鏋勫缓

.NOTES
    渚濊禆锛?
    - Visual Studio 2022/2019
    - WDK 10.0.22621.0
    - Musa.Runtime锛堜粨搴撹嚜甯︼級
#>

[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [switch]$Sign,
    [switch]$DriverOnly,
    [switch]$SkipDriver,
    [switch]$SkipCLI,
    [switch]$SkipGUI,
    [switch]$Package,
    [switch]$BuildRelease,
    [switch]$BuildR3,

    [string]$WdkVersion = $null
)

# Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 璇诲彇閰嶇疆鏂囦欢锛堝鏋滃瓨鍦級
function Read-BuildConfig {
    param([string]$ConfigFile)

    $config = @{}
    if (Test-Path $ConfigFile) {
        Get-Content $ConfigFile | ForEach-Object {
            $line = $_.Trim()
            if ($line -and -not $line.StartsWith('#')) {
                if ($line -match '^(\w+)\s*=\s*(.+)$') {
                    $config[$matches[1]] = $matches[2].Trim()
                }
            }
        }
    }
    return $config
}

# 鑾峰彇閰嶇疆鍊硷紙浼樺厛绾э細鍛戒护琛?> 鐜鍙橀噺 > 閰嶇疆鏂囦欢 > 榛樿鍊硷級
function Get-ConfigValue {
    param(
        [string]$ParamValue,
        [string]$EnvVarName,
        [hashtable]$ConfigFile,
        [string]$ConfigKey,
        [string]$DefaultValue
    )

    if ($ParamValue) { return $ParamValue }
    if ($EnvVarName) {
        $envEntry = Get-Item -Path ("Env:{0}" -f $EnvVarName) -ErrorAction SilentlyContinue
        if ($envEntry -and $envEntry.Value) {
            return $envEntry.Value
        }
    }
    if ($ConfigFile.ContainsKey($ConfigKey)) { return $ConfigFile[$ConfigKey] }
    return $DefaultValue
}

# 瀵煎叆鍏叡鍑芥暟搴?
$scriptDir = $PSScriptRoot
if (-not $scriptDir) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
}
$commonPath = Join-Path $scriptDir 'common.psm1'
if (-not (Test-Path -Path $commonPath)) {
    throw "Common functions module not found: $commonPath"
}
Import-Module $commonPath -Force

function Get-VsDevCmdPath {
    try {
        $msbuildPath = Get-MSBuildPath
    } catch {
        return $null
    }

    $binDir = Split-Path $msbuildPath
    $currentDir = Split-Path $binDir
    $msbuildRoot = Split-Path $currentDir
    $vsRoot = Split-Path $msbuildRoot
    $vsDevCmd = Join-Path $vsRoot 'Common7\Tools\VsDevCmd.bat'

    if (Test-Path -Path $vsDevCmd -PathType Leaf) {
        return $vsDevCmd
    }

    return $null
}

# 璇诲彇鏋勫缓閰嶇疆鏂囦欢
$buildConfig = Read-BuildConfig -ConfigFile (Join-Path $scriptDir 'build.config')

# 搴旂敤閰嶇疆锛堝鏋滄湭閫氳繃鍛戒护琛屾寚瀹氾級
if (-not $PSBoundParameters.ContainsKey('WdkVersion')) {
    $configWdkVersion = Get-ConfigValue `
        -ParamValue $null `
        -EnvVarName 'FCL_MUSA_WDK_VERSION' `
        -ConfigFile $buildConfig `
        -ConfigKey 'WdkVersion' `
        -DefaultValue $null

    if ($configWdkVersion) {
        $WdkVersion = $configWdkVersion
        Write-Verbose "Using WDK version from config/env: $WdkVersion"
    }
}

# 璁＄畻浠撳簱鏍圭洰褰曞拰鍏抽敭璺緞
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..')).ProviderPath
$kernelDir = Join-Path $repoRoot 'r0/driver/msbuild'
$outputDir = Join-Path $kernelDir "out\$Platform\$Configuration"
$driverSys = Join-Path $outputDir 'FclMusaDriver.sys'
$driverPdb = Join-Path $outputDir 'FclMusaDriver.pdb'
$distDriverDir = Join-Path $repoRoot "dist\driver\$Platform\$Configuration"
$r3BuildDir = Join-Path $repoRoot 'build\r3-demo'
$r3DemoExe = Join-Path (Join-Path $r3BuildDir $Configuration) 'FclMusaUserDemo.exe'

# 鍙傛暟楠岃瘉
if ($DriverOnly -and $SkipDriver) {
    throw "Cannot specify both -DriverOnly and -SkipDriver"
}

if ($DriverOnly -and ($SkipCLI -or $SkipGUI)) {
    Write-BuildWarning "-DriverOnly overrides -SkipCLI and -SkipGUI"
}

if ($SkipDriver -and $Sign) {
    throw "Cannot sign driver when -SkipDriver is specified"
}

if ($BuildRelease -and $SkipDriver) {
    Write-BuildWarning "-BuildRelease ignored because driver build is skipped."
    $BuildRelease = $false
}

if ($BuildRelease -and $Configuration -eq 'Release') {
    Write-BuildWarning "Already building Release configuration; ignoring -BuildRelease."
    $BuildRelease = $false
}

if ($DriverOnly -and $BuildR3) {
    Write-BuildWarning "-BuildR3 ignored because -DriverOnly is specified."
    $BuildR3 = $false
}

# 璁＄畻瑕佹墽琛岀殑姝ラ
$steps = @()
if (-not $SkipDriver) {
    $steps += "Driver"
    if ($BuildRelease) {
        $steps += "DriverRelease"
    }
    if ($Sign) {
        $steps += "Sign"
    }
}
if (-not $DriverOnly -and -not $SkipCLI) {
    $steps += "CLI"
}
if (-not $DriverOnly -and -not $SkipGUI) {
    $steps += "GUI"
}
if ($BuildR3) {
    $steps += "R3"
}
if ($Package) {
    $steps += "Package"
}

$totalSteps = $steps.Count
$currentStep = 0

Write-Host ""
Write-Host "FCL+Musa Build System" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration | $Platform" -ForegroundColor Gray
Write-Host "Steps:         $($steps -join ' 鈫?')" -ForegroundColor Gray
Write-Host ""

# ========================================
# 姝ラ 0: 鍑嗗渚濊禆
# ========================================
$currentStep++
Write-BuildStep $currentStep $totalSteps "Setting up dependencies"
$setupDepsScript = Join-Path $scriptDir 'setup_dependencies.ps1'
Invoke-BuildCommand `
    -ScriptBlock { & $setupDepsScript } `
    -ErrorMessage "Dependency setup failed"

# ========================================
# 姝ラ 1: 鏋勫缓椹卞姩
# ========================================
if ($steps -contains "Driver") {
    $currentStep++
    $signText = if ($Sign) { " + sign" } else { "" }
    Write-BuildStep $currentStep $totalSteps "Building kernel driver$signText ($Configuration|$Platform)"

    # 纭繚 Musa.Runtime 閰嶇疆瀛樺湪
    Ensure-MusaRuntimePublish -RepoRoot $repoRoot

    # 使用 Resolve-WdkEnvironment 自动探测 WDK
    $wdkParams = @{}
    if ($WdkVersion) { $wdkParams['RequestedVersion'] = $WdkVersion }
    $wdkInfo = Resolve-WdkEnvironment @wdkParams
    $actualWdkVersion = $wdkInfo.Version
    Write-Host "  Using WDK: $($wdkInfo.Version) ($($wdkInfo.Root))" -ForegroundColor Gray

    if ($wdkInfo.IncludePaths -and $wdkInfo.IncludePaths.Count -gt 0) {
        $env:INCLUDE = ($wdkInfo.IncludePaths -join ';' ) + ';' + $env:INCLUDE
    }
    if ($wdkInfo.LibPaths -and $wdkInfo.LibPaths.Count -gt 0) {
        $env:LIB = ($wdkInfo.LibPaths -join ';' ) + ';' + $env:LIB
    }
    if ($wdkInfo.BinPaths -and $wdkInfo.BinPaths.Count -gt 0) {
        $env:PATH = ($wdkInfo.BinPaths -join ';' ) + ';' + $env:PATH
    }

    # 浣跨敤 MSBuild 鐩存帴鏋勫缓
    $msbuild = Get-MSBuildPath
    $solutionFile = Join-Path $kernelDir 'FclMusaDriver.sln'

    # 鏋勫缓鍙傛暟
    $buildArgs = @(
        $solutionFile
        "/t:Build"
        "/p:Configuration=$Configuration"
        "/p:Platform=$Platform"
        "/p:WindowsTargetPlatformVersion=$actualWdkVersion"
        "/m"
        "/v:minimal"
        "/nologo"
    )

    Invoke-BuildCommand `
        -ScriptBlock { & $msbuild @buildArgs } `
        -ErrorMessage "Driver build failed"

    # 楠岃瘉椹卞姩鏂囦欢瀛樺湪
    if (-not (Test-Path -Path $driverSys -PathType Leaf)) {
        throw "Driver SYS not found after build: $driverSys"
    }

    Write-Host "  鉁?Driver built: $driverSys" -ForegroundColor Green
}

if ($steps -contains "DriverRelease") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Building kernel driver (Release via manual_build.cmd)"

    $manualBuildCmd = Join-Path $scriptDir 'manual_build.cmd'
    $previousWdkEnv = $env:FCL_MUSA_WDK_VERSION
    $wdkOverrideApplied = $false
    if ($WdkVersion) {
        $env:FCL_MUSA_WDK_VERSION = $WdkVersion
        $wdkOverrideApplied = $true
    }

    try {
        Invoke-BuildCommand `
            -ScriptBlock { cmd.exe /c "`"$manualBuildCmd`" Release" } `
            -ErrorMessage "Release driver build via manual_build.cmd failed"
    }
    finally {
        if ($wdkOverrideApplied) {
            if ([string]::IsNullOrWhiteSpace($previousWdkEnv)) {
                Remove-Item Env:FCL_MUSA_WDK_VERSION -ErrorAction SilentlyContinue
            } else {
                $env:FCL_MUSA_WDK_VERSION = $previousWdkEnv
            }
        }
    }

    $releaseDriverSys = Join-Path $kernelDir "out\$Platform\Release\FclMusaDriver.sys"
    if (Test-Path -Path $releaseDriverSys -PathType Leaf) {
        Write-Host "  鉁?Release driver built: $releaseDriverSys" -ForegroundColor Green
    } else {
        Write-BuildWarning "Release driver build finished but SYS not found: $releaseDriverSys"
    }
}

# ========================================
# 姝ラ 2: 绛惧悕椹卞姩锛堝鏋滈渶瑕侊級
# ========================================
if ($steps -contains "Sign") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Signing driver"

    $signDriverScript = Join-Path $scriptDir 'sign_driver.ps1'
    Invoke-BuildCommand `
        -ScriptBlock { & $signDriverScript -DriverPath $driverSys } `
        -ErrorMessage "sign_driver.ps1 failed"

    Write-Host "  鉁?Driver signed" -ForegroundColor Green

    # 澶嶅埗鍒?dist/ 鐩綍
    Write-Host "  鈫?Packaging to $distDriverDir" -ForegroundColor Gray
    Ensure-Directory $distDriverDir

    Copy-Item -Path $driverSys -Destination $distDriverDir -Force
    Copy-IfExists -Source $driverPdb -Destination $distDriverDir -Force | Out-Null

    $cerPath = Join-Path $outputDir 'FclMusaTestCert.cer'
    $pfxPath = Join-Path $outputDir 'FclMusaTestCert.pfx'
    Copy-IfExists -Source $cerPath -Destination $distDriverDir -Force | Out-Null
    Copy-IfExists -Source $pfxPath -Destination $distDriverDir -Force | Out-Null

    Write-Host "  鉁?Artifacts packaged to dist/driver/" -ForegroundColor Green
}

# ========================================
# 姝ラ 3: 鏋勫缓 CLI Demo
# ========================================
if ($steps -contains "CLI") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Building CLI demo"

    $buildDemoScript = Join-Path $scriptDir 'build_demo.ps1'
    Invoke-BuildCommand `
        -ScriptBlock { & $buildDemoScript -Configuration $Configuration } `
        -ErrorMessage "build_demo.ps1 failed"

    $cliExe = Join-Path $scriptDir 'build/fcl_demo.exe'
    if (Test-Path -Path $cliExe -PathType Leaf) {
        Write-Host "  鉁?CLI demo built: $cliExe" -ForegroundColor Green
    } else {
        Write-BuildWarning "CLI demo executable not found: $cliExe"
    }
}

# ========================================
# 姝ラ 4: 鏋勫缓 GUI Demo
# ========================================
if ($steps -contains "GUI") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Building GUI demo"

    $guiBuildCmd = Join-Path $scriptDir 'gui_demo\build_gui_demo.cmd'
    Invoke-BuildCommand `
        -ScriptBlock { cmd.exe /c "`"$guiBuildCmd`"" } `
        -ErrorMessage "build_gui_demo.cmd failed"

    $guiExe = Join-Path $scriptDir "gui_demo\build\Release\fcl_gui_demo.exe"
    if (Test-Path -Path $guiExe -PathType Leaf) {
        Write-Host "  鉁?GUI demo built: $guiExe" -ForegroundColor Green
    } else {
        Write-BuildWarning "GUI demo executable not found: $guiExe"
    }
}

# ========================================
# Step 5: Build R3 demo (user-mode only)
# ========================================
if ($steps -contains "R3") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Building R3 user-mode demo ($Configuration)"

    $vsDevCmd = Get-VsDevCmdPath
    if (-not $vsDevCmd) {
        throw "VsDevCmd.bat 未找到，请安装包含 C++ 工作负载的 Visual Studio。"
    }

    $cmakeGenerator = 'Visual Studio 17 2022'
    $cmakeConfigureCmd = 'cmake -S "{0}" -B "{1}" -G "{2}" -A x64 -DFCLMUSA_BUILD_DRIVER=OFF -DFCLMUSA_BUILD_KERNEL_LIB=OFF -DFCLMUSA_BUILD_USERLIB=ON' -f $repoRoot, $r3BuildDir, $cmakeGenerator
    $cmakeBuildCmd = 'cmake --build "{0}" --config {1} --target FclMusaUserDemo' -f $r3BuildDir, $Configuration
    $fullCmd = 'call "{0}" -arch=x64 -host_arch=x64 && {1} && {2}' -f $vsDevCmd, $cmakeConfigureCmd, $cmakeBuildCmd

    Invoke-BuildCommand `
        -ScriptBlock { cmd.exe /c $fullCmd } `
        -ErrorMessage "R3 demo build failed"

    if (Test-Path -Path $r3DemoExe -PathType Leaf) {
        Write-Host "  鉁?R3 demo built: $r3DemoExe" -ForegroundColor Green
    } else {
        Write-BuildWarning "R3 demo executable not found: $r3DemoExe"
    }
}

# ========================================
# Step 6: Packaging (if requested)
# ========================================
if ($steps -contains "Package") {
    $currentStep++
    Write-BuildStep $currentStep $totalSteps "Packaging bundle"

    $packageScript = Join-Path $scriptDir 'package_bundle.ps1'
    Invoke-BuildCommand `
        -ScriptBlock { & $packageScript -Configuration $Configuration -Platform $Platform } `
        -ErrorMessage "package_bundle.ps1 failed"

    $bundleDir = Join-Path $repoRoot "dist\bundle\$Platform\$Configuration"
    Write-Host "  鉁?Bundle packaged: $bundleDir" -ForegroundColor Green
}

# ========================================
# 鏋勫缓瀹屾垚鎬荤粨
# ========================================
Write-Host ""
Write-Success "鉁?Build completed successfully!"
Write-Host ""

# 杈撳嚭鏋勫缓浜х墿浣嶇疆
if ($steps -contains "Driver" -or $steps -contains "Sign") {
    $driverLocation = if ($steps -contains "Sign") {
        "dist\driver\$Platform\$Configuration\FclMusaDriver.sys (signed)"
    } else {
        "$outputDir\FclMusaDriver.sys (unsigned)"
    }
    Write-Host "Driver:  $driverLocation" -ForegroundColor Gray
}

if ($steps -contains "CLI") {
    Write-Host "CLI Demo: tools\build\fcl_demo.exe" -ForegroundColor Gray
}

if ($steps -contains "GUI") {
    Write-Host "GUI Demo: tools\gui_demo\build\Release\fcl_gui_demo.exe" -ForegroundColor Gray
}

if ($steps -contains "R3") {
    Write-Host "R3 Demo:  build\r3-demo\$Configuration\FclMusaUserDemo.exe" -ForegroundColor Gray
}

if ($steps -contains "Package") {
    Write-Host "Bundle:   dist\bundle\$Platform\$Configuration\" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
if ($steps -contains "Driver") {
    Write-Host "  1. Install driver: tools\manage_driver.ps1 -Action Install" -ForegroundColor Gray
    Write-Host "  2. Start driver:   tools\manage_driver.ps1 -Action Start" -ForegroundColor Gray
    Write-Host "  3. Run self-test:  tools\fcl-self-test.ps1" -ForegroundColor Gray
}
if ($steps -contains "CLI") {
    Write-Host "  4. Run CLI demo:   tools\build\fcl_demo.exe" -ForegroundColor Gray
}
if ($steps -contains "R3") {
    Write-Host "  5. Run R3 demo:   build\r3-demo\$Configuration\FclMusaUserDemo.exe" -ForegroundColor Gray
}
Write-Host ""

# Exit with success code
exit 0
