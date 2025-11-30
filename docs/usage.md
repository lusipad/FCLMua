# FCL+Musa 使用指南

面向已有 WDK 环境的开发者，帮助完成从构建到基本 IOCTL 操作的最小路径。

## 1. 环境准备

1. 安装 **Visual Studio 2022**（含 “使用 C++ 的桌面开发”）
2. 安装 **WDK 10.0.22621.0**
3. 克隆仓库：
   ```powershell
   git clone https://github.com/lusipad/FCLMua.git
   cd FCLMua
   ```

## 2. 构建驱动

### 2.1 推荐：一键构建所有组件

```powershell
PS> tools\build_all.ps1 -Configuration Release
```

- 自动构建驱动、CLI Demo、GUI Demo
- 打包所有产物到 `dist/bundle/x64/Release/`
- 包含驱动签名和证书生成

### 2.2 仅构建驱动

```powershell
# 构建 + 签名（推荐）
PS> tools\build_and_sign_driver.ps1 -Configuration Debug

# 仅构建（不签名，适合 CI）
PS> tools\manual_build.cmd
```

- 产物位于：`dist/driver/x64/{Debug|Release}/FclMusaDriver.sys`
- `build_and_sign_driver.ps1` 会自动生成/更新测试证书并签名
- `manual_build.cmd` 适合 CI 或自动化脚本

## 3. 部署与加载

### 方法一：使用管理脚本（推荐）

```powershell
# 管理员 PowerShell

# 安装并启动（默认使用 dist\driver\x64\Release\FclMusaDriver.sys）
PS> tools\manage_driver.ps1 -Action Install
PS> tools\manage_driver.ps1 -Action Start

# 或直接重启（会自动创建服务）
PS> tools\manage_driver.ps1 -Action Restart

# 卸载
PS> tools\manage_driver.ps1 -Action Uninstall
```

### 方法二：手动部署

1. 将 `FclMusaDriver.sys` 复制至目标机器（例如 `C:\Drivers`）
2. 若使用测试证书，导入 `FclMusaTestCert.cer` 到 **Trusted Root** 与 **Trusted Publisher**
   ```cmd
   certutil -addstore Root dist\driver\x64\Release\FclMusaTestCert.cer
   certutil -addstore TrustedPublisher dist\driver\x64\Release\FclMusaTestCert.cer
   ```
3. 创建服务并启动：
   ```cmd
   sc create FclMusa type= kernel binPath= C:\Drivers\FclMusaDriver.sys
   sc start FclMusa
   ```
4. 卸载：`sc stop FclMusa && sc delete FclMusa`

更多细节参考 `docs/deployment.md`。

## 4. 驱动初始化与 API

在驱动入口中：

```cpp
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status = FclInitialize();
    if (!NT_SUCCESS(status)) {
        return status;
    }
    DriverObject->DriverUnload = DriverUnload;
    return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT) {
    FclCleanup();
}
```

常用 API：`FclCreateGeometry` / `FclDestroyGeometry`、`FclCollideObjects`、`FclDistanceCompute`、`FclContinuousCollision` 等，全部在 `kernel/core/include/fclmusa/*.h` 中定义。

## 5. IOCTL 快速验证

1. 运行 `tools\build\fcl_demo.exe`（先 `tools\build_demo.cmd` 编译）
2. 典型操作：
   ```text
   > sphere a 0.5 0 0 0
   > sphere b 0.5 1 0 0
   > collide a b
   > distance a b
   > ccd a b 2 0 0
   > destroy a
   > list
   ```
3. 若需自动化验证，可使用：
   - `tools/fcl-self-test.ps1`：执行 `PING + SELF_TEST`
   - `tools/verify_upstream.ps1`：对比驱动输出与 upstream FCL

## 6. 常见问题

| 问题 | 处理办法 |
|------|----------|
| 构建时报找不到 WDK 头文件 | 确认已安装 WDK 10.0.22621.0，且 `tools/manual_build.cmd` 具备管理员权限 |
| 驱动加载失败，错误 577 | 系统未启用测试签名，或证书未导入 Trusted Root/Publisher |
| IOCTL 超时或错误 0xC0000008 | 设备名不匹配，确保 CreateFile 使用 `\\.\FclMusa`，并确认驱动正在运行 |
| 需要 Release 构建 | 在 VS 中切换到 Release|x64，或编辑脚本添加参数后再运行 |

## 7. 下一步

- 深入了解 IOCTL 结构：`docs/api.md`
- 参考 `docs/testing.md` 拓展自测项
- 结合 `docs/vm_debug_setup.md` 搭建 Hyper-V + WinDbg 远程调试环境
