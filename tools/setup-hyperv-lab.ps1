[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter()]
    [string]$VmName = 'FclKernelTest',

    [Parameter()]
    [string]$VmRoot = (Join-Path -Path $env:PUBLIC -ChildPath 'Documents/FclKernelLab'),

    [Parameter()]
    [string]$VhdPath,

    [Parameter()]
    [string]$ParentVhdPath,

    [Parameter()]
    [ValidateRange(32, 256)]
    [UInt64]$VhdSizeGB = 64,

    [Parameter()]
    [string]$SwitchName = 'FclKernelLabSwitch',

    [Parameter()]
    [string]$NatName = 'FclKernelLabNat',

    [Parameter()]
    [string]$Subnet = '192.168.251.0/24',

    [Parameter()]
    [ValidateRange(2147483648, 17179869184)]
    [UInt64]$MemoryStartupBytes = 4GB,

    [Parameter()]
    [ValidateRange(1, 16)]
    [int]$ProcessorCount = 4,

    [Parameter()]
    [ValidateRange(1024, 65535)]
    [int]$DebugPort = 50000,

    [Parameter()]
    [string]$DebugKey,

    [Parameter()]
    [string]$IsoPath,

    [Parameter()]
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]$identity
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw '必须在管理员 PowerShell 会话中运行此脚本。'
    }
}

function Ensure-HyperVFeature {
    $feature = Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All
    if ($feature.State -ne 'Enabled') {
        throw '尚未启用 Hyper-V，可通过 "Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V -All" 安装。'
    }
}

function Import-HyperVModule {
    if (-not (Get-Module -Name Hyper-V -ListAvailable)) {
        throw '未找到 Hyper-V PowerShell 模块。'
    }
    Import-Module Hyper-V -ErrorAction Stop | Out-Null
}

function ConvertTo-FullPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }
    $expanded = [Environment]::ExpandEnvironmentVariables($Path)
    return [System.IO.Path]::GetFullPath($expanded)
}

function Convert-IPv4ToUInt32 {
    param([string]$Address)
    try {
        $ip = [System.Net.IPAddress]::Parse($Address)
    } catch {
        throw "无效 IPv4 地址: $Address"
    }
    if ($ip.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
        throw "仅支持 IPv4: $Address"
    }
    $bytes = $ip.GetAddressBytes()
    [Array]::Reverse($bytes)
    return [BitConverter]::ToUInt32($bytes, 0)
}

function Convert-UInt32ToIPv4 {
    param([uint32]$Value)
    $bytes = [BitConverter]::GetBytes($Value)
    [Array]::Reverse($bytes)
    return ([System.Net.IPAddress]::new($bytes)).ToString()
}

function Get-NetworkPlan {
    param([string]$Cidr)
    $parts = $Cidr.Split('/', [System.StringSplitOptions]::RemoveEmptyEntries)
    if ($parts.Count -ne 2) {
        throw "子网格式应为 x.x.x.x/yy，当前: $Cidr"
    }
    $prefixLength = [int]$parts[1]
    if ($prefixLength -lt 20 -or $prefixLength -gt 28) {
        throw '推荐使用 /20~ /28 之间的私有网络以容纳调试流量。'
    }
    $networkValue = [uint64](Convert-IPv4ToUInt32 -Address $parts[0])
    $rangeSize = [uint64]1 -shl (32 - $prefixLength)
    $broadcastValue = $networkValue + $rangeSize - 1
    $hostMin = $networkValue + 1
    $hostMax = $broadcastValue - 1
    if ($hostMax -lt $hostMin + 9) {
        throw "子网 $Cidr 无法提供足够地址。"
    }
    $hostIp = Convert-UInt32ToIPv4([uint32]$hostMin)
    $sampleGuestIp = Convert-UInt32ToIPv4([uint32]($hostMin + 9))
    return [pscustomobject]@{
        Network       = Convert-UInt32ToIPv4([uint32]$networkValue)
        PrefixLength  = $prefixLength
        HostIp        = $hostIp
        SampleGuestIp = $sampleGuestIp
    }
}

function Ensure-VMSwitch {
    param([string]$Name)
    $switch = Get-VMSwitch -Name $Name -ErrorAction SilentlyContinue
    if (-not $switch) {
        New-VMSwitch -Name $Name -SwitchType Internal | Out-Null
    }
}

function Ensure-AdapterAddress {
    param(
        [string]$SwitchName,
        [string]$HostIp,
        [int]$PrefixLength,
        [switch]$Force
    )
    $alias = "vEthernet ($SwitchName)"
    $adapter = $null
    for ($i = 0; $i -lt 10; $i++) {
        $adapter = Get-NetAdapter -Name $alias -ErrorAction SilentlyContinue
        if ($adapter) {
            break
        }
        Start-Sleep -Seconds 1
    }
    if (-not $adapter) {
        throw "未找到 Hyper-V 虚拟网卡: $alias"
    }
    $existing = Get-NetIPAddress -InterfaceAlias $alias -AddressFamily IPv4 -ErrorAction SilentlyContinue
    $matched = $existing | Where-Object { $_.IPAddress -eq $HostIp -and $_.PrefixLength -eq $PrefixLength }
    if ($matched) {
        return
    }
    if ($existing -and -not $Force) {
        throw "网卡 $alias 已绑定 $($existing[0].IPAddress)/$($existing[0].PrefixLength)，使用 -Force 以覆盖。"
    }
    foreach ($address in $existing) {
        Remove-NetIPAddress -InputObject $address -Confirm:$false
    }
    New-NetIPAddress -InterfaceAlias $alias -IPAddress $HostIp -PrefixLength $PrefixLength | Out-Null
}

function Ensure-Nat {
    param(
        [string]$NatName,
        [string]$Subnet,
        [switch]$Force
    )
    $nat = Get-NetNat -Name $NatName -ErrorAction SilentlyContinue
    if ($nat -and $nat.InternalIPInterfaceAddressPrefix -ne $Subnet) {
        if (-not $Force) {
            throw "Nat $NatName 已存在且网段为 $($nat.InternalIPInterfaceAddressPrefix)，使用 -Force 以替换。"
        }
        Remove-NetNat -Name $NatName -Confirm:$false
        $nat = $null
    }
    if (-not $nat) {
        New-NetNat -Name $NatName -InternalIPInterfaceAddressPrefix $Subnet | Out-Null
    }
}

function Ensure-Vhd {
    param(
        [string]$VhdPath,
        [string]$ParentVhdPath,
        [UInt64]$VhdSizeGB
    )
    if (Test-Path -LiteralPath $VhdPath) {
        return
    }
    if ($ParentVhdPath) {
        if (-not (Test-Path -LiteralPath $ParentVhdPath)) {
            throw "找不到父 VHD: $ParentVhdPath"
        }
        New-VHD -Path $VhdPath -ParentPath $ParentVhdPath -Differencing | Out-Null
    } else {
        $sizeBytes = $VhdSizeGB * 1GB
        New-VHD -Path $VhdPath -Dynamic -SizeBytes $sizeBytes | Out-Null
    }
}

function Ensure-Vm {
    param(
        [string]$VmName,
        [string]$VhdPath,
        [string]$SwitchName,
        [UInt64]$MemoryStartupBytes,
        [switch]$Force
    )
    $vm = Get-VM -Name $VmName -ErrorAction SilentlyContinue
    if ($vm -and $vm.State -ne 'Off') {
        if (-not $Force) {
            throw "虚拟机 $VmName 正在运行，使用 -Force 以自动关闭。"
        }
        Stop-VM -VM $vm -TurnOff -Force
        $vm = Get-VM -Name $VmName -ErrorAction SilentlyContinue
    }
    if (-not $vm) {
        $vm = New-VM -Name $VmName -Generation 2 -MemoryStartupBytes $MemoryStartupBytes -VHDPath $VhdPath -SwitchName $SwitchName
    }
    return $vm
}

function Ensure-PrimaryDisk {
    param(
        [string]$VmName,
        [string]$VhdPath
    )
    $drive = Get-VMHardDiskDrive -VMName $VmName -ControllerType SCSI -ControllerNumber 0 -ControllerLocation 0 -ErrorAction SilentlyContinue
    if ($drive) {
        if ($drive.Path -ne $VhdPath) {
            Set-VMHardDiskDrive -VMName $VmName -ControllerType SCSI -ControllerNumber 0 -ControllerLocation 0 -Path $VhdPath
        }
    } else {
        Add-VMHardDiskDrive -VMName $VmName -ControllerType SCSI -ControllerNumber 0 -ControllerLocation 0 -Path $VhdPath | Out-Null
    }
}

function Ensure-AdapterMapping {
    param(
        [string]$VmName,
        [string]$SwitchName
    )
    $adapter = Get-VMNetworkAdapter -VMName $VmName | Select-Object -First 1
    if (-not $adapter) {
        Add-VMNetworkAdapter -VMName $VmName -SwitchName $SwitchName | Out-Null
        return
    }
    if ($adapter.SwitchName -ne $SwitchName) {
        Connect-VMNetworkAdapter -VMNetworkAdapter $adapter -SwitchName $SwitchName
    }
}

function Ensure-ProcessorAndMemory {
    param(
        [string]$VmName,
        [UInt64]$MemoryStartupBytes,
        [int]$ProcessorCount
    )
    Set-VMMemory -VMName $VmName -DynamicMemoryEnabled:$false -StartupBytes $MemoryStartupBytes -MinimumBytes $MemoryStartupBytes -MaximumBytes $MemoryStartupBytes | Out-Null
    Set-VMProcessor -VMName $VmName -Count $ProcessorCount | Out-Null
    Set-VM -Name $VmName -AutomaticStartAction Nothing -AutomaticStopAction Save -AutomaticCheckpointsEnabled:$false | Out-Null
}

function Ensure-DvdDrive {
    param(
        [string]$VmName,
        [string]$IsoPath
    )
    if (-not (Test-Path -LiteralPath $IsoPath)) {
        throw "找不到 ISO: $IsoPath"
    }
    $drive = Get-VMDvdDrive -VMName $VmName -ErrorAction SilentlyContinue
    if ($drive) {
        Set-VMDvdDrive -VMDvdDrive $drive -Path $IsoPath | Out-Null
    } else {
        Add-VMDvdDrive -VMName $VmName -Path $IsoPath | Out-Null
    }
}

function Ensure-ComPort {
    param([string]$VmName)
    $pipe = "\\.\pipe\${VmName}-kd"
    Set-VMComPort -VMName $VmName -Number 1 -Path $pipe | Out-Null
}

function New-DebugKey {
    $buffer = New-Object byte[] 16
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($buffer)
    return ($buffer | ForEach-Object { $_.ToString('X2') }) -join ''
}

function Get-WinDbgPath {
    $candidates = @(
        "$env:ProgramFiles\Windows Kits\10\Debuggers\x64\windbgx.exe",
        "$env:ProgramFiles\Windows Kits\10\Debuggers\x64\windbg.exe",
        "$env:ProgramFiles(x86)\Windows Kits\10\Debuggers\x64\windbgx.exe",
        "$env:ProgramFiles(x86)\Windows Kits\10\Debuggers\x64\windbg.exe"
    )
    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }
    return $null
}

Assert-Admin
Ensure-HyperVFeature
Import-HyperVModule

if ([string]::IsNullOrWhiteSpace($VmName)) {
    throw 'VmName 不能为空。'
}

if (-not $VhdPath) {
    $VhdPath = Join-Path -Path $VmRoot -ChildPath ("{0}.vhdx" -f $VmName)
}

$VmRoot = ConvertTo-FullPath $VmRoot
if (-not (Test-Path -LiteralPath $VmRoot)) {
    New-Item -ItemType Directory -Path $VmRoot -Force | Out-Null
}

$VhdPath = ConvertTo-FullPath $VhdPath
if ($ParentVhdPath) {
    $ParentVhdPath = ConvertTo-FullPath $ParentVhdPath
}
if ($IsoPath) {
    $IsoPath = ConvertTo-FullPath $IsoPath
}

if ([string]::IsNullOrWhiteSpace($DebugKey)) {
    $DebugKey = New-DebugKey
}

$networkPlan = Get-NetworkPlan -Cidr $Subnet

Ensure-VMSwitch -Name $SwitchName
Ensure-AdapterAddress -SwitchName $SwitchName -HostIp $networkPlan.HostIp -PrefixLength $networkPlan.PrefixLength -Force:$Force
Ensure-Nat -NatName $NatName -Subnet $Subnet -Force:$Force
Ensure-Vhd -VhdPath $VhdPath -ParentVhdPath $ParentVhdPath -VhdSizeGB $VhdSizeGB
Ensure-Vm -VmName $VmName -VhdPath $VhdPath -SwitchName $SwitchName -MemoryStartupBytes $MemoryStartupBytes -Force:$Force | Out-Null
Ensure-PrimaryDisk -VmName $VmName -VhdPath $VhdPath
Ensure-AdapterMapping -VmName $VmName -SwitchName $SwitchName
Ensure-ProcessorAndMemory -VmName $VmName -MemoryStartupBytes $MemoryStartupBytes -ProcessorCount $ProcessorCount
if ($IsoPath) {
    Ensure-DvdDrive -VmName $VmName -IsoPath $IsoPath
}
Ensure-ComPort -VmName $VmName

$windbgPath = Get-WinDbgPath
$pipePath = "\\.\pipe\${VmName}-kd"

$summary = [ordered]@{
    VmName        = $VmName
    VhdPath       = $VhdPath
    Switch        = $SwitchName
    NatName       = $NatName
    HostIp        = $networkPlan.HostIp
    SampleGuestIp = $networkPlan.SampleGuestIp
    PrefixLength  = $networkPlan.PrefixLength
    DebugPort     = $DebugPort
    DebugKey      = $DebugKey
    NamedPipe     = $pipePath
    WinDbg        = if ($windbgPath) { '"{0}" -k "net:port={1},key={2}"' -f $windbgPath, $DebugPort, $DebugKey } else { 'windbgx.exe -k "net:port={0},key={1}"' -f $DebugPort, $DebugKey }
}

$summary | Format-List

Write-Host ''
Write-Host 'Guest 静态网络配置示例:'
Write-Host ("  IP: {0}" -f $networkPlan.SampleGuestIp)
Write-Host ("  Mask: /{0}" -f $networkPlan.PrefixLength)
Write-Host ("  Gateway: {0}" -f $networkPlan.HostIp)
Write-Host ''
Write-Host 'Guest 启用 KDNET (管理员 CMD):'
Write-Host '  bcdedit /debug on'
Write-Host ("  bcdedit /dbgsettings net hostip={0} port={1} key={2}" -f $networkPlan.HostIp, $DebugPort, $DebugKey)
Write-Host ''
Write-Host 'Host 启动 WinDbg:'
Write-Host ("  {0}" -f $summary.WinDbg)
Write-Host ''
$selfTestPath = (Resolve-Path -LiteralPath (Join-Path -Path $PSScriptRoot -ChildPath 'fcl-self-test.ps1')).Path -replace '\\','/'
Write-Host '驱动上车后可在 Guest 执行:'
Write-Host '  pnputil /add-driver FclMusaDriver.inf /install'
Write-Host ("  pwsh -File {0}" -f $selfTestPath)
