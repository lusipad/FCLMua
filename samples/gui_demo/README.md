# FCL GUI Demo

> **快速指引** - 完整文档请查看 [docs/samples/gui_demo/](../../docs/samples/gui_demo/readme.md)

## 📦 快速构建

```powershell
# 从项目根目录运行
pwsh tools/build/build-tasks.ps1 -Task GUI-Demo
```

## 🚀 快速运行

```cmd
# 方法1: 使用便捷脚本
run.cmd

# 方法2: 直接运行
dist\gui_demo\fcl_gui_demo.exe
```

## 📚 详细文档

- **[完整文档](../../docs/samples/gui_demo/readme.md)** - 功能特性、系统要求、详细说明
- **[使用指南](../../docs/samples/gui_demo/usage.md)** - 操作演示、快速开始
- **[技术概览](../../docs/samples/gui_demo/overview.md)** - 架构设计、技术细节

## 🔧 常用脚本

| 脚本 | 说明 |
|------|------|
| `run.cmd` | 快速启动 GUI Demo |
| `clean.cmd` | 清理构建产物 |

## 💡 运行模式

```cmd
# Kernel 模式 (默认，需要驱动)
dist\gui_demo\fcl_gui_demo.exe

# R3 用户态模式 (无需驱动)
dist\gui_demo\fcl_gui_demo.exe --mode=r3

# 强制驱动模式
dist\gui_demo\fcl_gui_demo.exe --mode=driver
```

---

**项目地址**: [FCL+Musa](../..)
