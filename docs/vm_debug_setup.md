# Hyper-V + WinDbg 调试指南

用于快速搭建可调试的测试虚拟机，并与主机 WinDbg 建立 KDNET 连接。

## 1. 适用环境

- 主机：Windows 10/11 专业版（启用 Hyper-V）
- 需要管理员 PowerShell
- 已安装 WDK/WinDbg（`C:\Program Files (x86)\Windows Kits\10\Debuggers\x64`）

## 2. 自动创建虚拟机

执行脚本：
```powershell
pwsh -File tools\setup-hyperv-lab.ps1 `
    -VmName FclKernelTest `
    -Subnet 192.168.251.0/24 `
    -IsoPath "D:\ISO\Win11_24H2_Chinese(Simplified)_x64.iso"
```

脚本能力：

1. 检查 Hyper-V 组件与权限
2. 创建内部交换机与 NAT（默认 `FclKernelLabSwitch`/`FclKernelLabNat`）
3. 为 host 端 vEthernet 配置静态 IP（示例 `192.168.251.1/24`）
4. 生成动态 VHD（或基于 `-ParentVhdPath` 创建差分盘）
5. 创建 VM（Gen2、4GB RAM、4vCPU，可通过参数调整）
6. 配置调试端口、命名管道、虚拟 DVD（ISO）等
7. 输出 KDNET `bcdedit` 命令与 WinDbg 启动示例

常用参数：

- `-VmRoot`：存放 VHD/快照 的根目录
- `-MemoryStartupBytes`、`-ProcessorCount`
- `-DebugPort`、`-DebugKey` 自定义 KDNET 端口与密钥
- `-Force`：再次运行时覆盖现有配置

## 3. Guest 配置

1. 安装 Windows（从 ISO 启动）
2. 设置固定 IP（脚本输出 `192.168.251.10` 等）
3. 启用 KDNET：
   ```cmd
   bcdedit /debug on
   bcdedit /dbgsettings net hostip=192.168.251.1 port=50000 key=<脚本输出的key>
   ```
4. 安装 WDK 运行库/工具（可选）
5. 复制 `FclMusaDriver.sys` 并按 `docs/deployment.md` 加载驱动

## 4. WinDbg 附加

在主机运行：
```cmd
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbgx.exe" -k "net:port=50000,key=<key>"
```

建议在 WinDbg 中设置：

```text
!sym noisy
.sympath srv*C:\Symbols*https://msdl.microsoft.com/download/symbols
.reload
```

常用命令：`!analyze -v`、`!poolused 2 FCL`、`!verifier 0xA`、`!drvobj FclMusaDriver 7` 等。

## 5. Driver Verifier

在 Guest：
```cmd
verifier /standard /driver FclMusaDriver.sys
```

- 重启后 Driver Verifier 生效
- 运行 `IOCTL_FCL_SELF_TEST` 检查 `DriverVerifierActive = TRUE`
- 使用 WinDbg 观察内存/IRQL 问题

## 6. 常见问题

| 问题 | 解决方案 |
|------|----------|
| WinDbg 无法连接 | 检查防火墙、`bcdedit /dbgsettings` 是否使用最新 key；可尝试 `bcdedit /dbgsettings net hostip=<hostIP> port=<port> key=<key>` 重置 |
| Guest 无法访问网络 | 确认 NAT/内部交换机存在，必要时重新运行脚本并带 `-Force` |
| KDNET 连接后立即断开 | 关闭 VM 的自动休眠/节能设置，确保调试端口未被占用 |
| 需要串口备用链路 | 在 VM 设置中开启命名管道：`\\.\pipe\FclKernelTest-kd`，WinDbg 启动 `-k com:pipe,port=\\.\pipe\FclKernelTest-kd,resets=0` |

## 7. 建议流程

1. 创建基础 VM 快照（系统干净、已装调试工具）
2. 每次驱动验证前恢复快照，重新部署 `.sys`
3. 保留“出现 BSOD”快照便于复现与分析
4. 在长时间运行/压力测试前开启 Driver Verifier，以便提前发现资源泄漏

通过以上步骤，即可快速在 Hyper-V 环境中对 FCL+Musa 驱动进行可重复的调试与验证。
