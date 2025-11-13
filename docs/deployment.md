# 部署指引

本文描述如何在目标机器上部署 FCL+Musa 驱动、处理证书以及清理流程。

## 1. 准备文件

从构建目录复制下列文件到目标主机（示例路径 `C:\Drivers`）：

- `FclMusaDriver.sys`
- `FclMusaDriver.pdb`（可选，用于调试）
- 如果使用 `build_driver.cmd` 构建，还会生成 `FclMusaTestCert.pfx/.cer`

## 2. 证书处理

在测试环境中，通常使用 `FclMusaTestCert.cer` 作为驱动签名证书：

```cmd
certutil -addstore Root C:\Drivers\FclMusaTestCert.cer
certutil -addstore TrustedPublisher C:\Drivers\FclMusaTestCert.cer
```

可选：若需在不导入证书的情况下加载驱动，可启用测试签名模式：

```cmd
bcdedit /set testsigning on
```

（生产环境应使用正式证书并走 WHQL/EV 流程）

## 3. 创建并启动驱动服务

```cmd
sc create FclMusa type= kernel binPath= C:\Drivers\FclMusaDriver.sys
sc start FclMusa
```

- `sc start FclMusa` 返回 `SERVICE_RUNNING` 即表示加载成功
- 查看系统日志：`wevtutil qe System /q:"*[System[Provider[@Name='Service Control Manager'] and (EventID=7036)]]" /f:text /c:1`

## 4. 驱动管理

- 停止：`sc stop FclMusa`
- 删除服务：`sc delete FclMusa`
- 驱动卸载时会回调 `DriverUnload`，应确保 `FclCleanup()` 释放所有资源

## 5. WinDbg/Verifier 建议

1. **WinDbg KD**：在主机端运行 `windbgx -k net:port=<port>,key=<key>` 连接目标虚拟机，便于捕获 BSOD 或跟踪 `KdPrint`。
2. **Driver Verifier**：
   ```cmd
   verifier /standard /driver FclMusaDriver.sys
   ```
   - 建议在专门的测试 VM 中启用，并在 `IOCTL_FCL_SELF_TEST` 中检查 `DriverVerifierActive` 字段。

## 6. 清理

1. 停止并删除服务：`sc stop FclMusa && sc delete FclMusa`
2. 删除证书（如需）：
   ```cmd
   certutil -delstore Root FclMusaTestCert
   certutil -delstore TrustedPublisher FclMusaTestCert
   ```
3. 删除驱动文件与调试符号

## 7. 自动化脚本

- `tools/manual_build.cmd`：用于构建 `.sys`
- `tools/sign_driver.ps1`：`build_driver.cmd` 自动调用，支持 PFX 导出/签名
- 可结合 PowerShell 编写自定义部署脚本，典型步骤：复制文件 -> 导入证书 -> `sc create` -> `IOCTL_FCL_SELF_TEST` 验证

完成以上配置后，驱动即可在目标系统上运行，供内核模块或用户态 IOCTL 调用。
