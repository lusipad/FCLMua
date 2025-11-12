FCL+Musa 驱动示例
=================

## 1. Ring0 示例

下列示例演示如何在驱动代码（`driver_state.cpp` 等任意 PASSIVE_LEVEL 环境）调用 FCL+Musa 提供的 API 进行简单碰撞检测。示例基于当前仓库的头文件与实现，可直接复制到驱动入口或单元测试中运行。

```cpp
#include <ntddk.h>
#include "fclmusa/geometry.h"
#include "fclmusa/collision.h"

void DemoSphereCollision()
{
    // 1. 创建球体几何
    FCL_SPHERE_GEOMETRY_DESC sphereDesc = {};
    sphereDesc.Center = {0.0f, 0.0f, 0.0f};
    sphereDesc.Radius = 0.5f;

    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    NTSTATUS status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphereDesc, &sphereA);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Create sphere A failed 0x%X\n", status));
        return;
    }
    sphereDesc.Center.X = 1.0f;
    status = FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphereDesc, &sphereB);
    if (!NT_SUCCESS(status)) {
        FclDestroyGeometry(sphereA);
        KdPrint(("Create sphere B failed 0x%X\n", status));
        return;
    }

    // 2. 设置变换
    FCL_COLLISION_OBJECT_DESC objA = {};
    objA.Geometry = sphereA;
    objA.Transform = IdentityTransform();

    FCL_COLLISION_OBJECT_DESC objB = {};
    objB.Geometry = sphereB;
    objB.Transform = IdentityTransform();
    objB.Transform.Translation.X = 0.6f;  // 发生重叠

    // 3. 执行碰撞检测
    FCL_COLLISION_QUERY_RESULT queryResult = {};
    status = FclCollideObjects(&objA, &objB, nullptr, &queryResult);
    if (NT_SUCCESS(status) && queryResult.Intersecting) {
        const auto& contact = queryResult.Contact;
        KdPrint(("Collision detected: depth=%.3f normal=(%.3f, %.3f, %.3f)\n",
            contact.PenetrationDepth, contact.Normal.X, contact.Normal.Y, contact.Normal.Z));
    } else if (NT_SUCCESS(status)) {
        KdPrint(("No collision\n"));
    } else {
        KdPrint(("FclCollideObjects failed 0x%X\n", status));
    }

    // 4. 销毁几何
    FclDestroyGeometry(sphereB);
    FclDestroyGeometry(sphereA);
}

void DemoContinuousCollision()
{
    FCL_GEOMETRY_HANDLE sphere = {};
    FCL_SPHERE_GEOMETRY_DESC sphereDesc = {{0.0f, 0.0f, 0.0f}, 0.5f};
    if (!NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_SPHERE, &sphereDesc, &sphere))) {
        return;
    }

    // 球 1 从原点移动到 (2,0,0)
    FCL_INTERP_MOTION_DESC motionDesc = {};
    motionDesc.Start = IdentityTransform();
    motionDesc.End = IdentityTransform();
    motionDesc.End.Translation.X = 2.0f;
    FCL_INTERP_MOTION motionA = {};
    FclInterpMotionInitialize(&motionDesc, &motionA);

    // 球 2 静止在 x=1.0
    FCL_INTERP_MOTION motionB = {};
    motionDesc = {};
    motionDesc.Start = IdentityTransform();
    motionDesc.Start.Translation.X = 1.0f;
    motionDesc.End = motionDesc.Start;
    FclInterpMotionInitialize(&motionDesc, &motionB);

    FCL_CONTINUOUS_COLLISION_QUERY query = {};
    query.Object1 = sphere;
    query.Object2 = sphere;
    query.Motion1 = motionA;
    query.Motion2 = motionB;
    query.Tolerance = 1e-4;
    query.MaxIterations = 64;

    FCL_CONTINUOUS_COLLISION_RESULT result = {};
    if (NT_SUCCESS(FclContinuousCollision(&query, &result)) && result.Intersecting) {
        KdPrint(("TOI = %.4f, contact depth %.3f\n",
            result.TimeOfImpact, result.Contact.PenetrationDepth));
    }

    FclDestroyGeometry(sphere);
}
```

### 三角网格示例
```cpp
void DemoTriangleMesh()
{
    static const FCL_VECTOR3 vertices[] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    static const UINT32 indices[] = {
        0, 1, 2,
        0, 1, 3,
        0, 2, 3,
        1, 2, 3,
    };

    FCL_MESH_GEOMETRY_DESC desc = {};
    desc.Vertices = vertices;
    desc.VertexCount = RTL_NUMBER_OF(vertices);
    desc.Indices = indices;
    desc.IndexCount = RTL_NUMBER_OF(indices);

    FCL_GEOMETRY_HANDLE meshA = {};
    FCL_GEOMETRY_HANDLE meshB = {};
    if (!NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, &meshA))) {
        return;
    }
    if (!NT_SUCCESS(FclCreateGeometry(FCL_GEOMETRY_MESH, &desc, &meshB))) {
        FclDestroyGeometry(meshA);
        return;
    }

    FCL_TRANSFORM transformA = IdentityTransform();
    FCL_TRANSFORM transformB = IdentityTransform();
    transformB.Translation = {0.2f, 0.2f, 0.2f};

    BOOLEAN intersecting = FALSE;
    if (NT_SUCCESS(FclCollisionDetect(meshA, &transformA, meshB, &transformB, &intersecting, nullptr))) {
        KdPrint(("Mesh collision: %s\n", intersecting ? "YES" : "NO"));
    }

    FclDestroyGeometry(meshB);
    FclDestroyGeometry(meshA);
}
```

### 使用步骤
1. 确保驱动入口 (`DriverEntry`) 中已调用 `FclInitialize()` 成功。
2. 在 `PASSIVE_LEVEL` 上下文执行上述函数（如 IOCTL 处理、工作线程等）。
3. 驱动卸载前调用 `FclCleanup()`。
4. 若需要自测，可发出 `IOCTL_FCL_SELF_TEST`，查看 `FCL_SELF_TEST_RESULT` 中的字段。

更多 API 参见 `docs/api.md`；部署流程见 `docs/deployment.md`。

## 2. Ring3 控制程序

`tools/fcl_demo.cpp` 提供了一个用户态示例，演示如何通过 IOCTL 与驱动交互：

```powershell
PS> cd tools
PS> cl /EHsc /nologo /W4 fcl_demo.cpp
PS> .\fcl_demo.exe
```

示例 IOCTL：
- `IOCTL_FCL_SPHERE_COLLISION`：传入两个球体描述，驱动内部创建/销毁几何并返回碰撞结果。
- `IOCTL_FCL_CREATE_SPHERE` + `IOCTL_FCL_CONVEX_CCD`：先在驱动内创建几何句柄，再提交 InterpMotion，执行连续碰撞。

该示例默认设备名为 `\\.\FclMusa`，请确保驱动已创建相应符号链接并处于运行状态。如需根据实际名称修改，请编辑 `tools/fcl_demo.cpp` 中的 `CreateFileW` 调用。
