# FCL Collision Detection GUI Demo

这是一个Windows GUI上位机程序，用于可视化演示FCL碰撞检测功能。支持3D图形创建、实时碰撞检测、交互式操作（拖拽、旋转、缩放）。

## 功能特性

- **3D渲染引擎**: 基于Direct3D 11的高性能渲染
- **多种几何体**: 支持球体、立方体和三角面片网格
- **实时碰撞检测**: 与FclMusa内核驱动实时通信，检测物体碰撞
- **交互式操作**:
  - 相机旋转、平移、缩放
  - 物体拖拽
  - 物体旋转
  - 物体选择
- **可视化反馈**:
  - 选中物体高亮显示（黄色）
  - 碰撞物体高亮显示（红色）
  - 坐标轴Gizmo显示
  - 网格地面参考

## 系统要求

- Windows 10/11 (64位)
- Visual Studio 2019或更新版本（含C++工具）
- Direct3D 11支持的显卡
- FclMusa内核驱动（可选，用于实际碰撞检测）

## 编译

运行编译脚本：

```cmd
build_gui_demo.cmd
```

编译成功后，可执行文件位于：`build/Release/fcl_gui_demo.exe`

## 运行

### 方法1：连接驱动运行（完整功能）

1. 确保FclMusa驱动已加载：
   ```cmd
   sc start FclMusa
   ```

2. 运行GUI demo：
   ```cmd
   build\Release\fcl_gui_demo.exe
   ```

### 方法2：独立运行（仅UI测试）

直接运行，不连接驱动也可以测试UI和3D渲染功能：

```cmd
build\Release\fcl_gui_demo.exe
```

**注意**: 不连接驱动时，碰撞检测功能将不可用。

## 操作说明

### 相机控制

| 操作 | 说明 |
|------|------|
| **右键拖拽** | 旋转相机（Arc-ball旋转） |
| **中键拖拽** | 平移相机 |
| **鼠标滚轮** | 缩放相机（调整与目标的距离） |

### 物体操作

| 操作 | 说明 |
|------|------|
| **数字键 1-9** | 选择对应编号的物体 |
| **ESC** | 取消选择 |
| **左键拖拽** | 在XZ平面上拖动选中的物体 |
| **W/S 键** | 垂直移动选中的物体（上/下） |
| **Q/E 键** | 旋转选中的物体（逆时针/顺时针） |

### 创建和删除物体

| 操作 | 说明 |
|------|------|
| **Ctrl+C** | 在相机中心创建新球体 |
| **Ctrl+B** | 在相机中心创建新立方体 |
| **Delete** | 删除选中的物体 |

### 碰撞可视化

- **正常状态**: 物体以其原始颜色显示
  - 红色球体: Sphere 1
  - 蓝色球体: Sphere 2
  - 绿色立方体: Box 1
- **选中状态**: 物体显示为黄色
- **碰撞状态**: 发生碰撞的物体显示为红色

## 项目结构

```
gui_demo/
├── src/
│   ├── main.cpp              # 主程序入口
│   ├── window.h/cpp          # Win32窗口管理
│   ├── renderer.h/cpp        # D3D11渲染引擎
│   ├── camera.h/cpp          # 相机控制（Arc-ball）
│   ├── scene.h/cpp           # 场景管理和物体操作
│   └── fcl_driver.h/cpp      # FCL驱动通信（IOCTL）
├── build_gui_demo.cmd        # 编译脚本
└── README.md                 # 本文档
```

## 技术细节

### 渲染引擎

- **API**: Direct3D 11
- **着色器**: HLSL内联编译
- **光照**: 简单的方向光照明模型
- **几何体生成**: 程序化生成球体和立方体网格

### 相机系统

- **类型**: Arc-ball相机（球形坐标系）
- **参数**:
  - Yaw（偏航角）: 绕Y轴旋转
  - Pitch（俯仰角）: 绕X轴旋转
  - Distance（距离）: 相机到目标的距离
  - Target（目标点）: 观察中心

### 驱动通信

使用IOCTL接口与FclMusa内核驱动通信：

- `IOCTL_FCL_CREATE_SPHERE`: 创建球体几何
- `IOCTL_FCL_CREATE_MESH`: 创建三角面片网格
- `IOCTL_FCL_DESTROY_GEOMETRY`: 销毁几何对象
- `IOCTL_FCL_QUERY_COLLISION`: 查询碰撞
- `IOCTL_FCL_QUERY_DISTANCE`: 计算距离

### 碰撞检测

- **检测模式**: 全对比较（N²复杂度）
- **更新频率**: 每帧更新
- **结果**: 布尔碰撞状态 + 接触信息（穿透深度、法线等）

## 已知限制

1. **OBB支持**: 当前Box使用渲染表示，但未完全集成到FCL驱动（需要添加OBB创建IOCTL）
2. **网格加载**: 暂不支持从OBJ文件加载网格（可通过代码扩展）
3. **多选**: 暂不支持同时选择多个物体
4. **Gizmo**: 当前Gizmo为简单线条，未实现交互式变换控制

## 扩展建议

### 向导式创建界面（ImGui）

可集成ImGui实现图形化创建向导：

```cpp
// 球体创建向导
ImGui::Begin("Create Sphere");
static float radius = 1.0f;
static float position[3] = {0, 2, 0};
ImGui::InputFloat3("Position", position);
ImGui::SliderFloat("Radius", &radius, 0.1f, 5.0f);
if (ImGui::Button("Create")) {
    scene->AddSphere("New Sphere",
        XMFLOAT3(position[0], position[1], position[2]),
        radius);
}
ImGui::End();
```

### OBJ模型加载

可使用tinyobjloader库加载外部模型：

```cpp
#include "tiny_obj_loader.h"

void Scene::LoadObjFile(const std::string& path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    // ... 加载和转换
    AddMesh("Loaded Model", position, vertices, indices);
}
```

### 性能优化

- **宽阶段加速**: 使用空间分区减少碰撞检测对数
- **异步查询**: 将驱动IOCTL调用移到后台线程
- **LOD**: 根据相机距离使用不同细节级别的网格

## 故障排除

### 驱动连接失败

**问题**: "Failed to connect to FCL driver"

**解决方案**:
1. 检查驱动是否加载: `sc query FclMusa`
2. 尝试启动驱动: `sc start FclMusa`
3. 以管理员权限运行程序
4. 检查驱动设备路径是否正确: `\\\\.\\FclMusa`

### 编译错误

**问题**: "Cannot find Visual Studio"

**解决方案**:
1. 安装Visual Studio 2019或更新版本
2. 确保安装了"使用C++的桌面开发"工作负载
3. 手动运行vcvars64.bat后再编译

### 黑屏或崩溃

**问题**: 运行后窗口黑屏或崩溃

**解决方案**:
1. 更新显卡驱动
2. 检查是否支持Direct3D 11
3. 以兼容模式运行

## 参考资料

- [FCL文档](../../docs/api.md)
- [Direct3D 11编程指南](https://docs.microsoft.com/en-us/windows/win32/direct3d11/dx-graphics-overviews)
- [IOCTL参考](../../kernel/core/include/fclmusa/ioctl.h)

## 许可

本项目是FCLMua项目的一部分，遵循相同的许可协议。
