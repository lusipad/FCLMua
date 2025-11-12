FCL+Musa 部署指引
=================

1. 前置条件
------------
- Windows 10/11 x64 测试机
- 启用测试签名或已配置驱动签名证书
- 安装 WDK/VS 以便编译（若直接使用编译好的 .sys，可略过）

2. 构建
--------
```powershell
PS> cd D:\Repos\FCL+Musa
PS> .\build_driver.cmd
```
- 产物：`kernel/FclMusaDriver/out/x64/Debug/FclMusaDriver.sys`

3. 安装服务
-------------
```cmd
sc create FclMusa type= kernel binPath= C:\path\FclMusaDriver.sys
sc start FclMusa
```
- 需管理员权限

4. 调试/测试
--------------
- 运行 IOCTL（示例伪代码）：
```c
HANDLE h = CreateFile("\\\\.\\FclMusa", ...);
FCL_PING_RESPONSE ping = {};
DeviceIoControl(h, IOCTL_FCL_PING, nullptr, 0, &ping, sizeof(ping), ...);
```
- 自测：`IOCTL_FCL_SELF_TEST`

5. 卸载
--------
```cmd
sc stop FclMusa
sc delete FclMusa
```

6. 目录结构（文档/代码）
-----------------------
- `docs/api.md`：API 说明
- `docs/usage.md`：使用指南
- `docs/architecture.md`：架构概览
- `docs/testing.md`：测试报告
- `docs/known_issues.md`：已知问题
- `docs/release_notes.md`：发布说明

