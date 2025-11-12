FCL+Musa Driver API 文档
========================

## 初始化
### NTSTATUS FclInitialize();
- 初始化驱动内部子系统（内存跟踪、Musa.Runtime、自测等）。
- 已初始化再次调用返回 `STATUS_ALREADY_INITIALIZED`。

### VOID FclCleanup();
- 清理所有资源，停止内存跟踪。
- 建议在驱动卸载或测试完成后调用。

## 几何管理
### NTSTATUS FclCreateGeometry(FCL_GEOMETRY_TYPE type, const VOID* geometryDesc, FCL_GEOMETRY_HANDLE* handle);
- 创建 Sphere/OBB/Mesh 几何。
- 描述符：
  - Sphere: `FCL_SPHERE_GEOMETRY_DESC`
  - OBB: `FCL_OBB_GEOMETRY_DESC`
  - Mesh: `FCL_MESH_GEOMETRY_DESC`
- 返回句柄写入 `handle`。

### NTSTATUS FclDestroyGeometry(FCL_GEOMETRY_HANDLE handle);
- 销毁几何，需保证无活跃引用。

### NTSTATUS FclUpdateMeshGeometry(FCL_GEOMETRY_HANDLE handle, const FCL_MESH_GEOMETRY_DESC* desc);
- 复用 `handle` 更新 Mesh 数据，增量重建 BVH。

### BOOLEAN FclIsGeometryHandleValid(FCL_GEOMETRY_HANDLE handle);
- 简单校验句柄是否非零。

### NTSTATUS FclAcquireGeometryReference(FCL_GEOMETRY_HANDLE handle, FCL_GEOMETRY_REFERENCE* reference, FCL_GEOMETRY_SNAPSHOT* snapshot);
- 获取快照供碰撞/距离使用。完成后需 `FclReleaseGeometryReference`。

## 碰撞/距离 API
### NTSTATUS FclCollisionDetect(FCL_GEOMETRY_HANDLE object1, const FCL_TRANSFORM* transform1, ...);
- 逐对碰撞检测，支持 Sphere/OBB/Mesh，返回 `FCL_CONTACT_INFO`。

### NTSTATUS FclCollideObjects(const FCL_COLLISION_OBJECT_DESC* object1, const ...);
- 更高层接口，支持批量返回 `FCL_COLLISION_QUERY_RESULT`。

### NTSTATUS FclDistanceCompute(..., FCL_DISTANCE_RESULT* result);
- 返回最短距离及最近点。

## 连续碰撞 (CCD)
### NTSTATUS FclInterpMotionInitialize/ Evaluate(...);
- 线性+四元数插值运动。

### NTSTATUS FclScrewMotionInitialize/ Evaluate(...);
- 螺旋运动（轴、角速度、线速度）。

### NTSTATUS FclContinuousCollision(const FCL_CONTINUOUS_COLLISION_QUERY* query, FCL_CONTINUOUS_COLLISION_RESULT* result);
- 基于 Conservative Advancement，返回 TOI + contact。

## 自测与健康检查
### NTSTATUS FclRunSelfTest(FCL_SELF_TEST_RESULT* result);
- 执行所有自检（几何/碰撞/CCD/压力/Verifier 等），结果通过结构体字段返回。

### NTSTATUS FclQueryHealth(FCL_PING_RESPONSE* response);
- IOCTL `_FCL_PING` 使用此 API，返回版本、初始化状态、上次错误、内存统计等。

