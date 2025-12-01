# 测试与验证指引

目标：通过自检、回归、压力与对比测试，验证 FCL+Musa 驱动的稳定性与正确性。

## 1. 构建与部署

1. 运行 `tools/build/build-tasks.ps1 -Task R0-Debug` 或 `tools/build/build-tasks.ps1` 生成 Debug|x64 驱动
   产物：`dist/driver/x64/Debug/FclMusaDriver.sys`
2. 按 `docs/deployment.md` 将驱动加载到目标测试机
3. 确认设备名 `\\.\FclMusa` 可被打开

## 2. 内置自检（IOCTL）

### 2.1 Ping

```powershell
PS> tools\fcl-self-test.ps1 -DevicePath \\.\FclMusa -PingOnly
```
- 返回 `FCL_PING_RESPONSE`：版本号、初始化状态、池统计等

### 2.2 Self Test

```powershell
PS> tools\fcl-self-test.ps1 -DevicePath \\.\FclMusa
```
输出字段包含：
- `InitializeStatus / GeometryCreateStatus / CollisionStatus`…
- `DriverVerifierActive`、`LeakTestStatus`、`StressDurationMicroseconds` 等
- `Passed = TRUE` 说明所有子测试成功，`PoolBytesDelta` 应为 0

## 3. 场景验证

### 3.1 用户态 CLI

1. `tools\build_demo.cmd`
2. `tools\build\fcl_demo.exe` 连接驱动，执行以下命令：
   ```text
   > sphere a 0.5
   > sphere b 0.5 1 0 0
   > collide a b
   > distance a b
   > ccd a b 2 0 0
   > periodic a b 1000
   > periodic_stop
   > selftest
   > selftest sphere
   > selftest_dpc
   > diag
   ```
3. 观察输出是否与预期一致（碰撞/距离/TOI/周期检测）

### 3.2 自动回归脚本

```powershell
PS> tools\verify_upstream.ps1 -DevicePath \\.\FclMusa -Tolerance 1e-4
```
- 将驱动输出与 upstream FCL 记录的 JSON 数据逐条比较
- 适用于 CI，可加 `-Json` 获得机器可读结果

## 4. 压力与稳定性

| 项目 | 步骤 |
|------|------|
| Pool 泄漏检查 | 自检前后对比 `PoolBefore/PoolAfter` |
| Driver Verifier | `verifier /standard /driver FclMusaDriver.sys`，再运行自检 |
| 长时间运行 | 编写循环脚本反复创建/销毁几何、发起碰撞/距离 IOCTL |
| WinDbg 监控 | 使用 `!poolused 2 FCL`、`!verifier 0xA` 等命令观察状态 |

## 5. 输出信息收集

1. 将 `FCL_SELF_TEST_RESULT` 序列化保存，便于对比
2. 驱动日志可在 WinDbg 中查看（`DbgPrint` 输出）
3. 若发生异常，保留 `MEMORY.DMP` 与 `FclMusaDriver.pdb` 以便分析

## 6. 回归基线

每次合入前建议至少完成：

- `tools\build\build-tasks.ps1 -Task R0-Debug` 或 `tools\build\build-tasks.ps1` 构建成功
- `tools\fcl-self-test.ps1` 全部通过
- `tools\verify_upstream.ps1` 无偏差
- 关键 IOCTL（Create/Destroy/Collide/Distance/CCD/Periodic）在 CLI 或脚本中验证
- 周期碰撞测试（`selftest_dpc`）无错误

根据需求可叠加 WinDbg/Verifier/长时间压力测试，保证驱动在目标场景下稳定运行。
