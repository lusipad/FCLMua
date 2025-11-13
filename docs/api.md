FCL+Musa Driver API 文档
========================

> 说明：本文件描述的是第 1 层 **FCL 内核管理模块** 暴露的 C 接口。  
> IOCTL 层只负责把用户态 buffer 映射到这些 API，不添加额外控制逻辑。

## 初始化

### NTSTATUS FclInitialize();
- 初始化内部内存系统、Musa.Runtime、自测等子模块；
- 已初始化情况下再次调用返回 `STATUS_ALREADY_INITIALIZED`。

### VOID FclCleanup();
- 释放内部资源并停止内存统计；
- 驱动卸载或测试结束时调用。

## 几何管理

### NTSTATUS FclCreateGeometry(FCL_GEOMETRY_TYPE type, const VOID* geometryDesc, FCL_GEOMETRY_HANDLE* handle);
- 创建 Sphere / OBB / Mesh 几何对象；
- 参数：
  - Sphere: `FCL_SPHERE_GEOMETRY_DESC`
  - OBB: `FCL_OBB_GEOMETRY_DESC`
  - Mesh: `FCL_MESH_GEOMETRY_DESC`
- 输出：有效的 `FCL_GEOMETRY_HANDLE`。

### NTSTATUS FclDestroyGeometry(FCL_GEOMETRY_HANDLE handle);
- 销毁几何对象，确保无活动引用后释放。

### NTSTATUS FclUpdateMeshGeometry(FCL_GEOMETRY_HANDLE handle, const FCL_MESH_GEOMETRY_DESC* desc);
- 更新 Mesh 顶点/索引数据，并重建或更新内部 BVH。

### BOOLEAN FclIsGeometryHandleValid(FCL_GEOMETRY_HANDLE handle);
- 校验几何句柄是否已在几何管理模块中注册。

### NTSTATUS FclAcquireGeometryReference(FCL_GEOMETRY_HANDLE handle, FCL_GEOMETRY_REFERENCE* reference, FCL_GEOMETRY_SNAPSHOT* snapshot);
- 获取几何快照用于碰撞 / 距离 / CCD 计算；
- 持有 `FCL_GEOMETRY_REFERENCE` 期间几何不会被销毁；
- 使用完成后必须调用 `FclReleaseGeometryReference`。

## 碰撞 / 距离 API

### NTSTATUS FclCollisionDetect(FCL_GEOMETRY_HANDLE object1, const FCL_TRANSFORM* transform1, ...);
- 基于 upstream FCL 执行碰撞检测，支持 Sphere / OBB / Mesh；
- 输出 `FCL_CONTACT_INFO`，包含接触点、法线和穿透深度。

### NTSTATUS FclCollideObjects(const FCL_COLLISION_OBJECT_DESC* object1, const ...);
- 面向调用方的高阶接口，封装几何句柄 + 变换；
- 返回 `FCL_COLLISION_QUERY_RESULT`（是否相交 + 接触信息）。

### NTSTATUS FclDistanceCompute(..., FCL_DISTANCE_RESULT* result);
- 计算最短距离及最近点，基于 upstream FCL `distance` 算法；
- 输出 `FCL_DISTANCE_RESULT`。

## 连续碰撞（CCD）

### NTSTATUS FclInterpMotionInitialize(const FCL_INTERP_MOTION_DESC* desc, FCL_INTERP_MOTION* motion);
- 基于起止位姿构造线性插值运动描述。

### NTSTATUS FclInterpMotionEvaluate(const FCL_INTERP_MOTION* motion, double t, FCL_TRANSFORM* transform);
- 在 `[0,1]` 区间上评估插值运动，输出中间位姿。

### NTSTATUS FclScrewMotionInitialize(const FCL_SCREW_MOTION_DESC* desc, FCL_SCREW_MOTION* motion);
- 构造螺旋运动描述（起止位姿 + 轴 + 速度参数）。

### NTSTATUS FclScrewMotionEvaluate(const FCL_SCREW_MOTION* motion, double t, FCL_TRANSFORM* transform);
- 在 `[0,1]` 区间上评估螺旋运动。

### NTSTATUS FclContinuousCollision(const FCL_CONTINUOUS_COLLISION_QUERY* query, FCL_CONTINUOUS_COLLISION_RESULT* result);
- 基于 upstream FCL 的连续碰撞算法（Conservative Advancement 等）计算 TOI + 接触信息；
- `query->Tolerance` / `MaxIterations` 为 0 时，使用模块内的合理默认值。

## 自测与健康检查

### NTSTATUS FclRunSelfTest(FCL_SELF_TEST_RESULT* result);
- 执行端到端自测试（初始化 / 几何 / 碰撞 / CCD / 宽阶段 / Verifier 相关场景）；
- 将每个子场景结果以及整体状态写入 `FCL_SELF_TEST_RESULT`。

### NTSTATUS FclQueryHealth(FCL_PING_RESPONSE* response);
- 为 `IOCTL_FCL_PING` 提供实现；
- 返回驱动版本、初始化状态、最近错误、运行时间以及内存池统计等。

