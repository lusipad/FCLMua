# Hyper-V 测试虚拟机与 WinDbg 调试指南

## 目标
- 提供可重复的 Hyper-V 虚拟机环境（静态网络 + NAT）用于加载 `FclMusaDriver.sys`
- 固化 WinDbg KDNET 调试链路，确保随时可捕获 BSOD、池统计与 IOCTL 交互日志
- 与 `tools/fcl-self-test.ps1`、`build_driver.cmd` 协同，实现最小可执行验证闭环

## 先决条件
- Windows 10/11 专业版或更高版本，已启用 Hyper-V
- PowerShell 5.1+ 管理员会话（脚本自动调用 Hyper-V 与网络命令）
- 已安装 WDK 10.0.26100 + WinDbg（`C:\Program Files (x86)\Windows Kits\10\Debuggers\x64`）
- 可用的 Windows 10/11 安装 ISO，或现成基础 VHD（可选）

## 自动化脚本：`tools/setup-hyperv-lab.ps1`
1. 以管理员身份打开 PowerShell，运行：
   ```
   pwsh -File tools/setup-hyperv-lab.ps1 `
     -VmName FclKernelTest `
     -Subnet 192.168.251.0/24 `
     -IsoPath "D:/ISO/Win11_24H2_English_x64.iso"
   ```
2. 脚本行为：
   - 检查 Hyper-V 特性与模块
   - 创建内部交换机 + NAT（默认 `FclKernelLabSwitch` / `FclKernelLabNat`）
   - 为 host 端 `vEthernet (FclKernelLabSwitch)` 配置静态 IP（默认 `192.168.251.1/24`）
   - 生成 64GB 动态 VHD（或基于 `-ParentVhdPath` 创建差分盘）
   - 创建/更新 VM（Generation 2、4GB RAM、4 vCPU、固定启动内存、禁用自动快照）
   - 配置调试串口命名管道、（可选）加载 ISO，并输出 KDNET `bcdedit` 与 WinDbg 命令
3. 常用参数：
   - `-VmRoot`：VHD/快照输出根目录（默认 `%PUBLIC%\Documents\FclKernelLab`）
   - `-VhdPath`：自定义 VHD 位置
   - `-ParentVhdPath`：使用现有基础盘时传入
   - `-MemoryStartupBytes` / `-ProcessorCount`：资源配额
   - `-DebugPort` / `-DebugKey`：自定义 KDNET 监听参数
   - `-Force`：需要覆盖已有 NAT、网卡 IP 或强制关闭正在运行的 VM 时使用

## Guest 配置流程
1. 安装 Windows：
   - 首次启动从 ISO 引导完成系统安装
   - 安装后建议启用增强会话并更新到最新补丁
2. 静态网络（以脚本默认输出为例）：
   - IP：`192.168.251.10`
   - Mask：`255.255.255.0`
   - Gateway：`192.168.251.1`
   - DNS：`1.1.1.1` 或企业 DNS
3. 启用 KDNET（管理员 CMD）：
   ```
   bcdedit /debug on
   bcdedit /dbgsettings net hostip=192.168.251.1 port=50000 key=<脚本输出的Key>
   ```
   - 重启 Guest 后会自动开始网络内核调试监听
4. 安装必备工具：
   - 安装 WDK runtime 组件（可选，用于 WinDbg 扩展）
   - 部署 WinDbg 证书、Driver Verifier 等调试工具

## WinDbg 附加
- Host 端运行脚本输出的命令，例如：
  ```
  "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbgx.exe" -k "net:port=50000,key=ABCDEF..."
  ```
- 建议保存 WorkSpace，开启下列自动化：
  - `!sym noisy; .sympath srv*C:\Symbols*https://msdl.microsoft.com/download/symbols`
  - `!analyze -f`、`!poolused 2 FCL`、`!drvobj FclMusaDriver 7`
- 若 Guest 侧已配置命名管道串口，可在 WinDbg 中额外添加 `com:pipe,port=\\.\pipe\FclKernelTest-kd,resets=0` 作为备份路径

## 驱动部署与验证
1. 构建：
   ```
   build_driver.cmd
   ```
   输出 `kernel/FclMusaDriver/out/x64/Debug/FclMusaDriver.sys`
2. 复制驱动与 INF 至 Guest（`\\tsclient\`、`robocopy` 或 ISO 挂载）
3. 安装与加载：
   ```
   pnputil /add-driver FclMusaDriver.inf /install
   sc start FclMusaDriver
   ```
4. 自检：
   ```
   pwsh -File tools/fcl-self-test.ps1 -DevicePath \\.\FclMusa
   ```
   或在 WinDbg 中直接执行 `!ioctl`、`!irpfind` 观察 IOCTL 结果
5. 驱动卸载：
   ```
   sc stop FclMusaDriver
   pnputil /delete-driver FclMusaDriver.inf /uninstall /force
   ```
6. Pool 泄漏确认：`!poolused 2 FCL` 应为 0；若脚本输出 `PoolBytesDelta != 0`，需重新跑 `FclCleanup`

## 常见问题
- **脚本提示网卡/IP 冲突**：已有 `vEthernet (SwitchName)` 配置，重新执行并附加 `-Force`
- **WinDbg 无法连接**：确认 host firewall 放行 `DebugPort`，Guest `bcdedit /dbgsettings` 是否使用最新 key
- **Guest 无法访问外网**：运行 `Get-NetNatStaticMapping` 确认 NAT 存在；必要时使用 `New-NetNat` 重新初始化
- **构建产物无法复制**：启用增强会话共享或使用 `Set-VMIntegrationService -Name "Guest Services" -Enabled $true`

## 维护建议
- 使用 Hyper-V Checkpoint 记录“基础镜像”“安装驱动前”“BSOD 捕获”等关键节点
- `Driver Verifier` 建议在单独快照中开启，避免影响常规调试
- 通过 `tools/setup-hyperv-lab.ps1 -Force` 可重复同步最新网络/调试配置，脚本保持幂等
