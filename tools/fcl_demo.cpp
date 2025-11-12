#include <windows.h>
#include <cstdio>
#include <cstdint>

#define IOCTL_FCL_PING            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_SELF_TEST       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_DISTANCE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_SPHERE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_DESTROY_GEOMETRY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_SPHERE_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CONVEX_CCD       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

struct FCL_VECTOR3 {
    float X;
    float Y;
    float Z;
};

struct FCL_MATRIX3X3 {
    float M[3][3];
};

struct FCL_TRANSFORM {
    FCL_MATRIX3X3 Rotation;
    FCL_VECTOR3 Translation;
};

struct FCL_GEOMETRY_HANDLE {
    uint64_t Value;
};

struct FCL_SPHERE_GEOMETRY_DESC {
    FCL_VECTOR3 Center;
    float Radius;
};

struct FCL_CREATE_SPHERE_INPUT {
    FCL_SPHERE_GEOMETRY_DESC Desc;
};

struct FCL_CREATE_SPHERE_OUTPUT {
    FCL_GEOMETRY_HANDLE Handle;
};

struct FCL_DESTROY_INPUT {
    FCL_GEOMETRY_HANDLE Handle;
};

struct FCL_CONTACT_INFO {
    FCL_VECTOR3 PointOnObject1;
    FCL_VECTOR3 PointOnObject2;
    FCL_VECTOR3 Normal;
    float PenetrationDepth;
};

struct FCL_COLLISION_RESULT {
    uint8_t IsColliding;
    uint8_t Reserved[3];
    FCL_CONTACT_INFO Contact;
};

struct FCL_SPHERE_COLLISION_BUFFER {
    FCL_SPHERE_GEOMETRY_DESC SphereA;
    FCL_SPHERE_GEOMETRY_DESC SphereB;
    FCL_COLLISION_RESULT Result;
};

struct FCL_INTERP_MOTION {
    FCL_TRANSFORM Start;
    FCL_TRANSFORM End;
};

struct FCL_CONTINUOUS_COLLISION_RESULT {
    uint8_t Intersecting;
    double TimeOfImpact;
    FCL_CONTACT_INFO Contact;
};

struct FCL_CONVEX_CCD_BUFFER {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_INTERP_MOTION Motion1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_INTERP_MOTION Motion2;
    FCL_CONTINUOUS_COLLISION_RESULT Result;
};

static FCL_TRANSFORM IdentityTransform() {
    FCL_TRANSFORM t = {};
    t.Rotation.M[0][0] = 1.0f;
    t.Rotation.M[1][1] = 1.0f;
    t.Rotation.M[2][2] = 1.0f;
    t.Translation = {0.0f, 0.0f, 0.0f};
    return t;
}

bool SendIoctl(HANDLE device, DWORD code, void* buffer, DWORD size) {
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(device, code, buffer, size, buffer, size, &bytesReturned, nullptr)) {
        printf("DeviceIoControl failed (0x%08lX)\n", GetLastError());
        return false;
    }
    return true;
}

FCL_GEOMETRY_HANDLE CreateSphere(HANDLE device, float radius, float offsetX) {
    FCL_CREATE_SPHERE_INPUT input = {};
    input.Desc.Center = {offsetX, 0.0f, 0.0f};
    input.Desc.Radius = radius;
    FCL_CREATE_SPHERE_OUTPUT output = {};
    if (!SendIoctl(device, IOCTL_FCL_CREATE_SPHERE, &input, sizeof(output))) {
        return {};
    }
    printf("Sphere created: handle=%llu\n", static_cast<unsigned long long>(output.Handle.Value));
    return output.Handle;
}

void DestroyGeometry(HANDLE device, FCL_GEOMETRY_HANDLE handle) {
    if (handle.Value == 0) {
        return;
    }
    FCL_DESTROY_INPUT input = {handle};
    if (SendIoctl(device, IOCTL_FCL_DESTROY_GEOMETRY, &input, sizeof(input))) {
        printf("Handle %llu destroyed\n", static_cast<unsigned long long>(handle.Value));
    }
}

void DemoSphereCollision(HANDLE device) {
    FCL_SPHERE_COLLISION_BUFFER buffer = {};
    buffer.SphereA.Center = {0.0f, 0.0f, 0.0f};
    buffer.SphereA.Radius = 0.5f;
    buffer.SphereB.Center = {0.8f, 0.0f, 0.0f};
    buffer.SphereB.Radius = 0.5f;

    if (!SendIoctl(device, IOCTL_FCL_SPHERE_COLLISION, &buffer, sizeof(buffer))) {
        return;
    }

    printf("Sphere collision result: %s\n", buffer.Result.IsColliding ? "Intersecting" : "Separated");
    if (buffer.Result.IsColliding) {
        printf("  Penetration depth: %.3f\n", buffer.Result.Contact.PenetrationDepth);
        printf("  Normal: (%.3f, %.3f, %.3f)\n",
               buffer.Result.Contact.Normal.X,
               buffer.Result.Contact.Normal.Y,
               buffer.Result.Contact.Normal.Z);
    }
}

void DemoConvexCcd(HANDLE device) {
    FCL_GEOMETRY_HANDLE sphereA = CreateSphere(device, 0.5f, 0.0f);
    FCL_GEOMETRY_HANDLE sphereB = CreateSphere(device, 0.5f, 2.0f);
    if (sphereA.Value == 0 || sphereB.Value == 0) {
        DestroyGeometry(device, sphereA);
        DestroyGeometry(device, sphereB);
        return;
    }

    FCL_CONVEX_CCD_BUFFER buffer = {};
    buffer.Object1 = sphereA;
    buffer.Object2 = sphereB;
    buffer.Motion1.Start = IdentityTransform();
    buffer.Motion1.End = IdentityTransform();
    buffer.Motion1.End.Translation.X = 2.0f;
    buffer.Motion2.Start = IdentityTransform();
    buffer.Motion2.End = IdentityTransform();

    if (SendIoctl(device, IOCTL_FCL_CONVEX_CCD, &buffer, sizeof(buffer))) {
        printf("CCD: %s, TOI=%.4f\n",
               buffer.Result.Intersecting ? "Intersecting" : "Separated",
               buffer.Result.TimeOfImpact);
    }

    DestroyGeometry(device, sphereA);
    DestroyGeometry(device, sphereB);
}

int main() {
    HANDLE device = CreateFileW(L"\\\\.\\FclMusa",
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (device == INVALID_HANDLE_VALUE) {
        printf("Failed to open \\\\.\\FclMusa (0x%08lX)\n", GetLastError());
        return 1;
    }

    printf("=== Demo: Sphere Collision ===\n");
    DemoSphereCollision(device);

    printf("\n=== Demo: Continuous Collision ===\n");
    DemoConvexCcd(device);

    CloseHandle(device);
    return 0;
}
