# FCL GUI 改进计划

## 目标描述
解决用户报告的 FCL GUI 问题：
1.  **UI 布局**：修复简单的布局以及窗口移动时文字/图形重叠的问题。
2.  **功能**：改进碰撞效果的可视化。
3.  **性能**：优化渲染和处理流程。

## 建议的更改

### UI 美化与现代化设计
![UI Mockup](../../docs/samples/gui_demo/images/ui_mockup.png)

#### [MODIFY] [window.cpp](src/window.cpp)
- **暗色主题**：将窗口背景设置为深灰色 (RGB 30, 30, 30)，文字设置为浅灰色/白色。
- **扁平化按钮**：
    - 将所有按钮样式修改为 `BS_OWNERDRAW`。
    - 实现 `WM_DRAWITEM` 消息处理，使用 GDI 绘制扁平化、无边框的现代风格按钮。
    - 添加鼠标悬停效果（Hover state）以增加交互感。
    - 使用强调色（如蓝色 RGB 0, 120, 215）作为主操作按钮的背景。
- **控件样式优化**：
    - 处理 `WM_CTLCOLOREDIT` 和 `WM_CTLCOLORSTATIC`，使输入框和标签适配暗色背景。
    - 增加控件间距，优化分组布局，去除老式的 `GROUPBOX` 边框，改用标题+分割线的设计。

### UI 布局与响应式修复
#### [MODIFY] [window.cpp](src/window.cpp)
- 实现 `OnResize` 回调机制以通知 `main.cpp`。
- 更新 `WM_SIZE` 处理程序以动态重新定位所有 UI 控件（使用相对定位）。
- 修复 `m_overlayLabel` 和 `m_statusPanel` 的定位。

#### [MODIFY] [main.cpp](src/main.cpp)
- 将 `Window` 的调整大小事件连接到 `Renderer::Resize`。

### 功能：碰撞可视化
#### [MODIFY] [scene.h](src/scene.h)
- 添加 `std::vector<ContactPoint> m_contacts` 以存储碰撞细节。
- 添加 `struct ContactPoint` 定义。

#### [MODIFY] [scene.cpp](src/scene.cpp)
- 更新 `DetectCollisions` 以存储接触点（位置、法线、深度）。
- 实现 `RenderContacts` 以可视化接触点（例如，黄色小球体 + 法线）。
- 在 `Render` 中调用 `RenderContacts`。

### 车辆模型美化
![Vehicle Concepts](../../docs/samples/gui_demo/images/vehicle_concepts.png)

#### [MODIFY] [scene.cpp](src/scene.cpp)
- 重写 `CreateVehicleMesh` 函数，不再使用简单的 Box 堆叠。
- **程序化生成细节**：
    - **车身流线**：使用梯形而非矩形来模拟挡风玻璃和后窗的倾斜。
    - **车窗**：通过顶点颜色区分车窗（浅蓝色）和车身。
    - **车灯**：添加前灯（白色/黄色）和尾灯（红色）的几何面。
    - **轮拱**：在车身侧面添加简单的轮拱凹陷感。
    - **车轮**：使用更圆滑的圆柱体近似（增加分段数）。
- 针对不同车型（轿车、SUV、卡车、跑车）定制不同的几何参数，使其特征更鲜明。

### 性能优化
#### [MODIFY] [scene.cpp](src/scene.cpp)
- 在 `DetectCollisions` 中实现客户端 Broadphase（AABB 检查）。
- 仅当 AABB 重叠时才调用 `m_driver->QueryCollision`（内核转换）。
- 这将显著减少 `DeviceIoControl` 调用的开销，解决“性能低”的问题。

## 验证计划

### 手动验证
1.  **UI 外观**：
    - 启动程序，确认界面为暗色主题。
    - 确认按钮为扁平化设计，鼠标悬停有变色反馈。
    - 确认整体视觉风格不再是传统的 Windows 95/2000 灰色风格。
2.  **UI 布局**：
    - 调整窗口大小，验证 3D 视图无拉伸，左侧面板控件自适应高度或保持固定宽度。
3.  **碰撞可视化**：
    - 创建两个球体并碰撞，验证接触点和法线的显示。
4.  **性能**：
    - 在“高”性能模式下运行复杂场景，验证 FPS 提升。
