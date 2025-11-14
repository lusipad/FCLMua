#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#define IOCTL_FCL_PING                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_SELF_TEST             CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_SELF_TEST_SCENARIO    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_DIAGNOSTICS     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_COLLISION       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_DISTANCE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_SPHERE         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_DESTROY_GEOMETRY      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_MESH           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x814, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CONVEX_CCD            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x815, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_START_PERIODIC_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x820, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_STOP_PERIODIC_COLLISION  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x821, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_DEMO_SPHERE_COLLISION  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

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

struct FCL_DRIVER_VERSION {
    uint32_t Major;
    uint32_t Minor;
    uint32_t Patch;
    uint32_t Build;
};

struct FCL_POOL_STATS {
    uint64_t AllocationCount;
    uint64_t FreeCount;
    uint64_t BytesAllocated;
    uint64_t BytesFreed;
    uint64_t BytesInUse;
    uint64_t PeakBytesInUse;
};

struct FCL_PING_RESPONSE {
    FCL_DRIVER_VERSION Version;
    uint8_t IsInitialized;
    uint8_t IsInitializing;
    uint16_t Reserved0;
    uint32_t Reserved;
    int32_t LastError;
    uint32_t Padding;  // align Uptime100ns to 8-byte boundary like kernel
    int64_t Uptime100ns;
    FCL_POOL_STATS Pool;
};
static_assert(sizeof(FCL_PING_RESPONSE) == 88, "Unexpected FCL_PING_RESPONSE size");

struct FCL_DETECTION_TIMING_STATS {
    uint64_t CallCount;
    uint64_t TotalDurationMicroseconds;
    uint64_t MinDurationMicroseconds;
    uint64_t MaxDurationMicroseconds;
};

struct FCL_DIAGNOSTICS_RESPONSE {
    FCL_DETECTION_TIMING_STATS Collision;
    FCL_DETECTION_TIMING_STATS Distance;
    FCL_DETECTION_TIMING_STATS ContinuousCollision;
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

struct FCL_COLLISION_QUERY {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM Transform2;
};

struct FCL_COLLISION_IO_BUFFER {
    FCL_COLLISION_QUERY Query;
    FCL_COLLISION_RESULT Result;
};

struct FCL_DISTANCE_QUERY {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM Transform2;
};

struct FCL_DISTANCE_OUTPUT {
    float Distance;
    FCL_VECTOR3 ClosestPoint1;
    FCL_VECTOR3 ClosestPoint2;
};

struct FCL_DISTANCE_IO_BUFFER {
    FCL_DISTANCE_QUERY Query;
    FCL_DISTANCE_OUTPUT Result;
};

struct FCL_INTERP_MOTION {
    FCL_TRANSFORM Start;
    FCL_TRANSFORM End;
};

struct FCL_CONTINUOUS_COLLISION_RESULT {
    uint8_t Intersecting;
    uint8_t Padding[7];
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

struct FCL_CREATE_MESH_BUFFER {
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t Reserved0;
    uint32_t Reserved1;
    FCL_GEOMETRY_HANDLE Handle;
};

struct FCL_PERIODIC_COLLISION_CONFIG {
    FCL_GEOMETRY_HANDLE Object1;
    FCL_TRANSFORM       Transform1;
    FCL_GEOMETRY_HANDLE Object2;
    FCL_TRANSFORM       Transform2;
    uint32_t            PeriodMicroseconds;
    uint32_t            Reserved[3];
};

struct SceneObject {
    std::string Name;
    FCL_GEOMETRY_HANDLE Handle{};
    FCL_TRANSFORM Transform{};
    std::string Type;
    size_t VertexCount = 0;
    size_t IndexCount = 0;
};

struct FCL_CONTACT_SUMMARY {
    FCL_VECTOR3 PointOnObject1;
    FCL_VECTOR3 PointOnObject2;
    FCL_VECTOR3 Normal;
    float PenetrationDepth;
};

enum class FCL_SELF_TEST_SCENARIO_ID : uint32_t {
    Runtime = 1,
    SphereCollision = 2,
    Broadphase = 3,
    MeshCollision = 4,
    Ccd = 5,
};

struct FCL_SELF_TEST_SCENARIO_REQUEST {
    uint32_t ScenarioId;
    uint32_t Reserved[3];
};

struct FCL_SELF_TEST_SCENARIO_RESULT {
    uint32_t ScenarioId;
    int32_t Status;
    uint32_t Step;
    uint32_t Reserved[3];
    FCL_POOL_STATS PoolBefore;
    FCL_POOL_STATS PoolAfter;
    FCL_CONTACT_SUMMARY Contact;
};
static_assert(sizeof(FCL_SELF_TEST_SCENARIO_REQUEST) % sizeof(uint32_t) == 0, "Unexpected FCL_SELF_TEST_SCENARIO_REQUEST size");
static_assert(sizeof(FCL_SELF_TEST_SCENARIO_RESULT) % sizeof(uint32_t) == 0, "Unexpected FCL_SELF_TEST_SCENARIO_RESULT size");

struct FCL_SELF_TEST_RESULT {
    FCL_DRIVER_VERSION Version;
    int32_t InitializeStatus;
    int32_t GeometryCreateStatus;
    int32_t CollisionStatus;
    int32_t DestroyStatus;
    int32_t DistanceStatus;
    int32_t BroadphaseStatus;
    int32_t MeshGjkStatus;
    int32_t SphereMeshStatus;
    int32_t MeshBroadphaseStatus;
    int32_t ContinuousCollisionStatus;
    int32_t GeometryUpdateStatus;
    int32_t SphereObbStatus;
    int32_t MeshComplexStatus;
    int32_t BoundaryStatus;
    int32_t DriverVerifierStatus;
    uint8_t DriverVerifierActive;
    uint8_t ReservedFlags[3];
    int32_t LeakTestStatus;
    int32_t StressStatus;
    int32_t PerformanceStatus;
    uint64_t StressDurationMicroseconds;
    uint64_t PerformanceDurationMicroseconds;
    int32_t OverallStatus;
    uint8_t Passed;
    uint8_t PoolBalanced;
    uint8_t CollisionDetected;
    uint8_t BoundaryPassed;
    uint16_t Reserved;
    int32_t InvalidGeometryStatus;
    int32_t DestroyInvalidStatus;
    int32_t CollisionInvalidStatus;
    uint64_t PoolBytesDelta;
    float DistanceValue;
    uint32_t BroadphasePairCount;
    uint32_t MeshBroadphasePairCount;
    FCL_POOL_STATS PoolBefore;
    FCL_POOL_STATS PoolAfter;
    FCL_CONTACT_SUMMARY Contact;
};
static_assert(sizeof(FCL_SELF_TEST_RESULT) == 296, "Unexpected FCL_SELF_TEST_RESULT size");

using SceneMap = std::unordered_map<std::string, SceneObject>;

static bool g_DpcBaselineValid = false;
static FCL_DIAGNOSTICS_RESPONSE g_DpcBaselineDiag = {};
static bool g_PassBaselineValid = false;
static FCL_DIAGNOSTICS_RESPONSE g_PassBaselineDiag = {};

FCL_TRANSFORM IdentityTransform() {
    FCL_TRANSFORM t = {};
    t.Rotation.M[0][0] = 1.0f;
    t.Rotation.M[1][1] = 1.0f;
    t.Rotation.M[2][2] = 1.0f;
    t.Translation = {0.0f, 0.0f, 0.0f};
    return t;
}

bool SendIoctl(
    HANDLE device,
    DWORD code,
    const void* inputBuffer,
    DWORD inSize,
    void* outputBuffer,
    DWORD outSize) {
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
            device,
            code,
            const_cast<void*>(inputBuffer),
            inSize,
            outputBuffer,
            outSize,
            &bytesReturned,
            nullptr)) {
        printf("DeviceIoControl failed (code=0x%08lX, error=0x%08lX)\n", code, GetLastError());
        return false;
    }
    return true;
}

bool RunSelfTestScenario(HANDLE device, const std::string& name) {
    FCL_SELF_TEST_SCENARIO_ID scenarioId;
    if (name == "runtime") {
        scenarioId = FCL_SELF_TEST_SCENARIO_ID::Runtime;
    } else if (name == "sphere") {
        scenarioId = FCL_SELF_TEST_SCENARIO_ID::SphereCollision;
    } else if (name == "broadphase") {
        scenarioId = FCL_SELF_TEST_SCENARIO_ID::Broadphase;
    } else if (name == "mesh") {
        scenarioId = FCL_SELF_TEST_SCENARIO_ID::MeshCollision;
    } else if (name == "ccd") {
        scenarioId = FCL_SELF_TEST_SCENARIO_ID::Ccd;
    } else {
        printf("Unknown scenario '%s'. Supported: runtime|sphere|broadphase|mesh|ccd\n", name.c_str());
        return true;
    }

    FCL_SELF_TEST_SCENARIO_REQUEST request = {};
    request.ScenarioId = static_cast<uint32_t>(scenarioId);

    FCL_SELF_TEST_SCENARIO_RESULT result = {};
    if (!SendIoctl(device, IOCTL_FCL_SELF_TEST_SCENARIO, &request, sizeof(request), &result, sizeof(result))) {
        printf("  [FAIL] IOCTL_FCL_SELF_TEST_SCENARIO failed.\n");
        return true;
    }

    printf("Scenario '%s' (id=%u): Status=0x%08X\n",
           name.c_str(),
           result.ScenarioId,
           result.Status);
    printf("  PoolBefore.BytesInUse=%llu PoolAfter.BytesInUse=%llu\n",
           static_cast<unsigned long long>(result.PoolBefore.BytesInUse),
           static_cast<unsigned long long>(result.PoolAfter.BytesInUse));
    printf("  Contact: P1=(%.3f, %.3f, %.3f) P2=(%.3f, %.3f, %.3f) N=(%.3f, %.3f, %.3f) Depth=%.6f\n",
           result.Contact.PointOnObject1.X, result.Contact.PointOnObject1.Y, result.Contact.PointOnObject1.Z,
           result.Contact.PointOnObject2.X, result.Contact.PointOnObject2.Y, result.Contact.PointOnObject2.Z,
           result.Contact.Normal.X, result.Contact.Normal.Y, result.Contact.Normal.Z,
           result.Contact.PenetrationDepth);

    return true;
}

void PrintTimingStats(const char* name, const FCL_DETECTION_TIMING_STATS& stats) {
    if (stats.CallCount == 0) {
        printf("  %-20s: no samples\n", name);
        return;
    }

    const double avg = static_cast<double>(stats.TotalDurationMicroseconds) / static_cast<double>(stats.CallCount);
    printf("  %-20s: calls=%llu total=%.3f ms min=%.3f ms max=%.3f ms avg=%.3f ms\n",
           name,
           static_cast<unsigned long long>(stats.CallCount),
           static_cast<double>(stats.TotalDurationMicroseconds) / 1000.0,
           static_cast<double>(stats.MinDurationMicroseconds) / 1000.0,
           static_cast<double>(stats.MaxDurationMicroseconds) / 1000.0,
           avg / 1000.0);
}

bool SendIoctl(HANDLE device, DWORD code, void* buffer, DWORD size) {
    return SendIoctl(device, code, buffer, size, buffer, size);
}

std::vector<std::string> Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    for (char ch : line) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

bool ParseFloat(const std::string& token, float& value) {
    try {
        value = std::stof(token);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseInt(const std::string& token, int& value) {
    try {
        value = std::stoi(token);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseFaceIndex(const std::string& token, size_t vertexCount, uint32_t& out) {
    if (token.empty()) {
        return false;
    }
    std::string number = token;
    const size_t slash = token.find('/');
    if (slash != std::string::npos) {
        number = token.substr(0, slash);
    }
    if (number.empty()) {
        return false;
    }
    try {
        int idx = std::stoi(number);
        if (idx > 0) {
            if (static_cast<size_t>(idx) <= vertexCount) {
                out = static_cast<uint32_t>(idx - 1);
                return true;
            }
        } else if (idx < 0) {
            const int rel = static_cast<int>(vertexCount) + idx;
            if (rel >= 0) {
                out = static_cast<uint32_t>(rel);
                return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool LoadObjMesh(const std::filesystem::path& path,
                 std::vector<FCL_VECTOR3>& vertices,
                 std::vector<uint32_t>& indices,
                 std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "Failed to open OBJ file: " + path.string();
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        if (token == "v") {
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (!(iss >> x >> y >> z)) {
                error = "Failed to parse OBJ vertex line.";
                return false;
            }
            vertices.push_back({x, y, z});
        } else if (token == "f") {
            std::vector<uint32_t> face;
            std::string entry;
            while (iss >> entry) {
                uint32_t idx = 0;
                if (!ParseFaceIndex(entry, vertices.size(), idx)) {
                    error = "OBJ face contains invalid indices.";
                    return false;
                }
                face.push_back(idx);
            }
            if (face.size() < 3) {
                error = "OBJ face contains fewer than 3 vertices.";
                return false;
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                indices.push_back(face[0]);
                indices.push_back(face[i]);
                indices.push_back(face[i + 1]);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        error = "OBJ file is missing vertices or indices.";
        return false;
    }
    return true;
}

bool CreateSphere(HANDLE device, float radius, const FCL_VECTOR3& center, FCL_GEOMETRY_HANDLE& handle) {
    FCL_CREATE_SPHERE_INPUT input = {};
    input.Desc.Center = center;
    input.Desc.Radius = radius;
    FCL_CREATE_SPHERE_OUTPUT output = {};
    if (!SendIoctl(
            device,
            IOCTL_FCL_CREATE_SPHERE,
            &input,
            sizeof(input),
            &output,
            sizeof(output))) {
        return false;
    }
    handle = output.Handle;
    return handle.Value != 0;
}

bool CreateMesh(HANDLE device,
                const std::vector<FCL_VECTOR3>& vertices,
                const std::vector<uint32_t>& indices,
                FCL_GEOMETRY_HANDLE& handle) {
    if (vertices.empty() || indices.size() < 3 || (indices.size() % 3) != 0) {
        printf("Mesh data is invalid (needs >= 1 triangle).\n");
        return false;
    }
    const size_t verticesSize = vertices.size() * sizeof(FCL_VECTOR3);
    const size_t indicesSize = indices.size() * sizeof(uint32_t);
    size_t totalSize = sizeof(FCL_CREATE_MESH_BUFFER);
    if (verticesSize > std::numeric_limits<size_t>::max() - totalSize) {
        printf("Mesh vertex data is too large.\n");
        return false;
    }
    totalSize += verticesSize;
    if (indicesSize > std::numeric_limits<size_t>::max() - totalSize) {
        printf("Mesh index data is too large.\n");
        return false;
    }
    totalSize += indicesSize;
    if (totalSize > std::numeric_limits<DWORD>::max()) {
        printf("Mesh buffer exceeds DeviceIoControl limit.\n");
        return false;
    }

    std::vector<uint8_t> buffer(totalSize);
    auto* header = reinterpret_cast<FCL_CREATE_MESH_BUFFER*>(buffer.data());
    header->VertexCount = static_cast<uint32_t>(vertices.size());
    header->IndexCount = static_cast<uint32_t>(indices.size());
    header->Reserved0 = header->Reserved1 = 0;
    header->Handle.Value = 0;

    auto* vertexDst = reinterpret_cast<FCL_VECTOR3*>(buffer.data() + sizeof(FCL_CREATE_MESH_BUFFER));
    memcpy(vertexDst, vertices.data(), verticesSize);
    auto* indexDst = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(vertexDst) + verticesSize);
    memcpy(indexDst, indices.data(), indicesSize);

    if (!SendIoctl(device,
                   IOCTL_FCL_CREATE_MESH,
                   buffer.data(),
                   static_cast<DWORD>(buffer.size()),
                   buffer.data(),
                   sizeof(FCL_CREATE_MESH_BUFFER))) {
        return false;
    }
    handle = header->Handle;
    return handle.Value != 0;
}

bool DestroyGeometry(HANDLE device, FCL_GEOMETRY_HANDLE handle) {
    if (handle.Value == 0) {
        return true;
    }
    FCL_DESTROY_INPUT input = {handle};
    return SendIoctl(device, IOCTL_FCL_DESTROY_GEOMETRY, &input, sizeof(input));
}

bool QueryCollision(HANDLE device, const SceneObject& a, const SceneObject& b, FCL_COLLISION_RESULT& result) {
    FCL_COLLISION_IO_BUFFER buffer = {};
    buffer.Query.Object1 = a.Handle;
    buffer.Query.Transform1 = a.Transform;
    buffer.Query.Object2 = b.Handle;
    buffer.Query.Transform2 = b.Transform;
    if (!SendIoctl(device, IOCTL_FCL_QUERY_COLLISION, &buffer, sizeof(buffer))) {
        return false;
    }
    result = buffer.Result;
    return true;
}

bool QueryDistance(HANDLE device, const SceneObject& a, const SceneObject& b, FCL_DISTANCE_OUTPUT& output) {
    FCL_DISTANCE_IO_BUFFER buffer = {};
    buffer.Query.Object1 = a.Handle;
    buffer.Query.Transform1 = a.Transform;
    buffer.Query.Object2 = b.Handle;
    buffer.Query.Transform2 = b.Transform;
    if (!SendIoctl(device, IOCTL_FCL_QUERY_DISTANCE, &buffer, sizeof(buffer))) {
        return false;
    }
    output = buffer.Result;
    return true;
}

bool RunConvexCcd(HANDLE device, const SceneObject& moving, const SceneObject& target, const FCL_VECTOR3& delta, double& toi) {
    FCL_CONVEX_CCD_BUFFER buffer = {};
    buffer.Object1 = moving.Handle;
    buffer.Object2 = target.Handle;
    buffer.Motion1.Start = moving.Transform;
    buffer.Motion1.End = moving.Transform;
    buffer.Motion1.End.Translation.X += delta.X;
    buffer.Motion1.End.Translation.Y += delta.Y;
    buffer.Motion1.End.Translation.Z += delta.Z;
    buffer.Motion2.Start = target.Transform;
    buffer.Motion2.End = target.Transform;
    if (!SendIoctl(device, IOCTL_FCL_CONVEX_CCD, &buffer, sizeof(buffer))) {
        return false;
    }
    toi = buffer.Result.TimeOfImpact;
    return buffer.Result.Intersecting != 0;
}

void PrintCollisionInfo(const SceneObject& a, const SceneObject& b, const FCL_COLLISION_RESULT& result) {
    printf("%s vs %s -> %s\n",
           a.Name.c_str(),
           b.Name.c_str(),
           result.IsColliding ? "Intersecting" : "Separated");
    if (result.IsColliding) {
        const auto& contact = result.Contact;
        printf("  Penetration: %.4f\n", contact.PenetrationDepth);
        printf("  Normal     : (%.4f, %.4f, %.4f)\n",
               contact.Normal.X,
               contact.Normal.Y,
               contact.Normal.Z);
    }
}

void PrintDistanceInfo(const SceneObject& a, const SceneObject& b, const FCL_DISTANCE_OUTPUT& output) {
    printf("%s vs %s -> Distance %.4f\n",
           a.Name.c_str(),
           b.Name.c_str(),
           output.Distance);
    printf("  Closest A: (%.4f, %.4f, %.4f)\n",
           output.ClosestPoint1.X,
           output.ClosestPoint1.Y,
           output.ClosestPoint1.Z);
    printf("  Closest B: (%.4f, %.4f, %.4f)\n",
           output.ClosestPoint2.X,
           output.ClosestPoint2.Y,
           output.ClosestPoint2.Z);
}

void ListObjects(const SceneMap& objects) {
    if (objects.empty()) {
        printf("No registered geometries.\n");
        return;
    }
    for (const auto& [name, obj] : objects) {
        printf("[%s] handle=%llu type=%s pos=(%.3f, %.3f, %.3f)\n",
               name.c_str(),
               static_cast<unsigned long long>(obj.Handle.Value),
               obj.Type.c_str(),
               obj.Transform.Translation.X,
               obj.Transform.Translation.Y,
               obj.Transform.Translation.Z);
    }
}

bool SelfTestPass(HANDLE device) {
    FCL_DIAGNOSTICS_RESPONSE baseline = {};
    if (!SendIoctl(device, IOCTL_FCL_QUERY_DIAGNOSTICS, &baseline, sizeof(baseline))) {
        printf("  [FAIL] IOCTL_FCL_QUERY_DIAGNOSTICS failed (baseline for selftest_pass).\n");
        return true;
    }
    g_PassBaselineDiag = baseline;
    g_PassBaselineValid = true;

    FCL_GEOMETRY_HANDLE handleA = {};
    FCL_GEOMETRY_HANDLE handleB = {};

    // 几何本地中心固定在原点，位置差异通过变换表示，保持与内核 self_test 一致
    FCL_VECTOR3 localCenter = {0.0f, 0.0f, 0.0f};
    if (!CreateSphere(device, 0.5f, localCenter, handleA)) {
        printf("  [FAIL] selftest_pass: failed to create sphere A.\n");
        return true;
    }
    if (!CreateSphere(device, 0.5f, localCenter, handleB)) {
        printf("  [FAIL] selftest_pass: failed to create sphere B.\n");
        DestroyGeometry(device, handleA);
        return true;
    }

    SceneObject a{};
    a.Name = "PassA";
    a.Handle = handleA;
    a.Transform = IdentityTransform();
    a.Transform.Translation = {0.0f, 0.0f, 0.0f};

    SceneObject b{};
    b.Name = "PassB";
    b.Handle = handleB;
    b.Transform = IdentityTransform();
    b.Transform.Translation = {0.6f, 0.0f, 0.0f};

    const int iterations = 640;
    printf("Running PASSIVE collision self-test: two spheres, iterations=%d...\n", iterations);

    FCL_COLLISION_RESULT result = {};
    for (int i = 0; i < iterations; ++i) {
        result = {};
        if (!QueryCollision(device, a, b, result)) {
            printf("  [FAIL] selftest_pass: QueryCollision failed at iteration %d.\n", i);
            break;
        }
        if (!result.IsColliding) {
            printf("  [FAIL] selftest_pass: expected collision at iteration %d.\n", i);
            break;
        }
    }

    DestroyGeometry(device, handleB);
    DestroyGeometry(device, handleA);

    printf("PASSIVE collision self-test completed. Use 'diag_pass' to view delta diagnostics.\n");
    return true;
}

bool PrintDiagPass(HANDLE device) {
    if (!g_PassBaselineValid) {
        printf("diag_pass: baseline not set. Run 'selftest_pass' first.\n");
        return true;
    }

    FCL_DIAGNOSTICS_RESPONSE now = {};
    if (!SendIoctl(device, IOCTL_FCL_QUERY_DIAGNOSTICS, &now, sizeof(now))) {
        printf("  [FAIL] IOCTL_FCL_QUERY_DIAGNOSTICS failed in diag_pass.\n");
        return true;
    }

    const auto& base = g_PassBaselineDiag;

    const uint64_t deltaCalls =
        (now.Collision.CallCount >= base.Collision.CallCount)
            ? (now.Collision.CallCount - base.Collision.CallCount)
            : 0;
    const uint64_t deltaTotal =
        (now.Collision.TotalDurationMicroseconds >= base.Collision.TotalDurationMicroseconds)
            ? (now.Collision.TotalDurationMicroseconds - base.Collision.TotalDurationMicroseconds)
            : 0;

    printf("PASSIVE collision diagnostics (delta since selftest_pass):\n");
    if (deltaCalls == 0) {
        printf("  Collision: no additional samples recorded.\n");
        return true;
    }

    const double avgUs = static_cast<double>(deltaTotal) / static_cast<double>(deltaCalls);
    printf("  Collision: calls=%llu total=%.3f ms avg=%.3f ms (global min=%.3f ms max=%.3f ms)\n",
           static_cast<unsigned long long>(deltaCalls),
           static_cast<double>(deltaTotal) / 1000.0,
           avgUs / 1000.0,
           static_cast<double>(now.Collision.MinDurationMicroseconds) / 1000.0,
           static_cast<double>(now.Collision.MaxDurationMicroseconds) / 1000.0);
    return true;
}
bool SelfTestDpc(HANDLE device) {
    FCL_DIAGNOSTICS_RESPONSE baseline = {};
    if (!SendIoctl(device, IOCTL_FCL_QUERY_DIAGNOSTICS, &baseline, sizeof(baseline))) {
        printf("  [FAIL] IOCTL_FCL_QUERY_DIAGNOSTICS failed (baseline for selftest_dpc).\n");
        return true;
    }
    g_DpcBaselineDiag = baseline;
    g_DpcBaselineValid = true;

    FCL_GEOMETRY_HANDLE handleA = {};
    FCL_GEOMETRY_HANDLE handleB = {};

    // 与 PASSIVE 自测保持一致：几何本地中心固定在原点，位移由变换提供
    FCL_VECTOR3 localCenter = {0.0f, 0.0f, 0.0f};
    if (!CreateSphere(device, 0.5f, localCenter, handleA)) {
        printf("  [FAIL] selftest_dpc: failed to create sphere A.\n");
        return true;
    }
    if (!CreateSphere(device, 0.5f, localCenter, handleB)) {
        printf("  [FAIL] selftest_dpc: failed to create sphere B.\n");
        DestroyGeometry(device, handleA);
        return true;
    }

    FCL_TRANSFORM tA = IdentityTransform();
    FCL_TRANSFORM tB = IdentityTransform();
    tA.Translation = {0.0f, 0.0f, 0.0f};
    tB.Translation = {0.6f, 0.0f, 0.0f};

    // 使用更密集的周期：1ms，一共约 64 次调用
    const uint32_t periodUs = 1000;    // 1ms
    const uint32_t innerIterations = 16;
    const uint32_t durationMs = 640;   // ~640 次调用

    FCL_PERIODIC_COLLISION_CONFIG config = {};
    config.Object1 = handleA;
    config.Transform1 = tA;
    config.Object2 = handleB;
    config.Transform2 = tB;
    config.PeriodMicroseconds = periodUs;
    config.Reserved[0] = innerIterations;

    if (!SendIoctl(
            device,
            IOCTL_FCL_START_PERIODIC_COLLISION,
            &config,
            static_cast<DWORD>(sizeof(config)),
            &config,
            static_cast<DWORD>(sizeof(config)))) {
        printf("  [FAIL] IOCTL_FCL_START_PERIODIC_COLLISION failed in selftest_dpc.\n");
        DestroyGeometry(device, handleB);
        DestroyGeometry(device, handleA);
        return true;
    }

    printf("Running DPC periodic self-test: two spheres, period=%u us, duration=%u ms...\n",
           static_cast<unsigned>(periodUs),
           static_cast<unsigned>(durationMs));

    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));

    (void)SendIoctl(
        device,
        IOCTL_FCL_STOP_PERIODIC_COLLISION,
        nullptr,
        0,
        nullptr,
        0);

    DestroyGeometry(device, handleB);
    DestroyGeometry(device, handleA);

    printf("DPC periodic self-test completed. Use 'diag_dpc' to view delta diagnostics.\n");
    return true;
}

bool PrintDiagDpc(HANDLE device) {
    if (!g_DpcBaselineValid) {
        printf("diag_dpc: baseline not set. Run 'selftest_dpc' first.\n");
        return true;
    }

    FCL_DIAGNOSTICS_RESPONSE now = {};
    if (!SendIoctl(device, IOCTL_FCL_QUERY_DIAGNOSTICS, &now, sizeof(now))) {
        printf("  [FAIL] IOCTL_FCL_QUERY_DIAGNOSTICS failed in diag_dpc.\n");
        return true;
    }

    const auto& base = g_DpcBaselineDiag;

    const uint64_t deltaCalls =
        (now.Collision.CallCount >= base.Collision.CallCount)
            ? (now.Collision.CallCount - base.Collision.CallCount)
            : 0;
    const uint64_t deltaTotal =
        (now.Collision.TotalDurationMicroseconds >= base.Collision.TotalDurationMicroseconds)
            ? (now.Collision.TotalDurationMicroseconds - base.Collision.TotalDurationMicroseconds)
            : 0;

    printf("DPC periodic collision diagnostics (delta since selftest_dpc):\n");
    if (deltaCalls == 0) {
        printf("  Collision: no additional samples recorded.\n");
        return true;
    }

    const double avgUs = static_cast<double>(deltaTotal) / static_cast<double>(deltaCalls);
    printf("  Collision: calls=%llu total=%.3f ms avg=%.3f ms (global min=%.3f ms max=%.3f ms)\n",
           static_cast<unsigned long long>(deltaCalls),
           static_cast<double>(deltaTotal) / 1000.0,
           avgUs / 1000.0,
           static_cast<double>(now.Collision.MinDurationMicroseconds) / 1000.0,
           static_cast<double>(now.Collision.MaxDurationMicroseconds) / 1000.0);
    return true;
}

bool StartPeriodicCollision(HANDLE device, const SceneObject& objectA, const SceneObject& objectB, uint32_t periodMicroseconds) {
    if (periodMicroseconds == 0) {
        printf("Period must be > 0 microseconds.\n");
        return false;
    }

    FCL_PERIODIC_COLLISION_CONFIG config = {};
    config.Object1 = objectA.Handle;
    config.Transform1 = objectA.Transform;
    config.Object2 = objectB.Handle;
    config.Transform2 = objectB.Transform;
    config.PeriodMicroseconds = periodMicroseconds;

    if (!SendIoctl(
            device,
            IOCTL_FCL_START_PERIODIC_COLLISION,
            &config,
            static_cast<DWORD>(sizeof(config)),
            &config,
            static_cast<DWORD>(sizeof(config)))) {
        printf("  [FAIL] IOCTL_FCL_START_PERIODIC_COLLISION failed.\n");
        return false;
    }

    printf("Started periodic collision: %s vs %s, period=%u us\n",
           objectA.Name.c_str(),
           objectB.Name.c_str(),
           static_cast<unsigned>(periodMicroseconds));
    printf("Use 'diag' to inspect aggregated timing stats while it runs.\n");
    return true;
}

bool StopPeriodicCollision(HANDLE device) {
    // 此 IOCTL 不检查缓冲区长度，传入空缓冲区即可。
    if (!SendIoctl(
            device,
            IOCTL_FCL_STOP_PERIODIC_COLLISION,
            nullptr,
            0,
            nullptr,
            0)) {
        printf("  [FAIL] IOCTL_FCL_STOP_PERIODIC_COLLISION failed.\n");
        return false;
    }

    printf("Stopped periodic collision.\n");
    return true;
}

void PrintHelp() {
    printf("Commands:\n");
    printf("  help                                 Show this message\n");
    printf("  run <script_path>                    Execute commands from script\n");
    printf("  load <name> <obj_path>               Load mesh from OBJ file\n");
    printf("  sphere <name> <radius> [x y z]       Create a sphere\n");
    printf("  move <name> <x> <y> <z>              Update translation\n");
    printf("  collide <nameA> <nameB>              Run discrete collision test\n");
    printf("  distance <nameA> <nameB>             Compute closest distance\n");
    printf("  simulate <mov> <static> <dx dy dz> <steps> <interval_ms>\n");
    printf("  ccd <mov> <static> <dx dy dz>        Run convex CCD query\n");
    printf("  periodic <mov> <static> <period_us>  Start periodic collision (DPC+PASSIVE)\n");
    printf("  periodic_stop                        Stop periodic collision\n");
    printf("  selftest_pass                        Run PASSIVE collision self-test (multi-call)\n");
    printf("  selftest_dpc                         Run periodic DPC self-test (DPC+PASSIVE)\n");
    printf("  destroy <name>                       Destroy a geometry\n");
    printf("  list                                 List registered geometries\n");
    printf("  demo                                 Run legacy sphere demo\n");
    printf("  selftest                             Run driver self-test (ping + IOCTL_FCL_SELF_TEST)\n");
    printf("  selftest <scenario>                  Run single self-test scenario (runtime|sphere|broadphase|mesh|ccd)\n");
    printf("  diag                                 Query kernel detection timing diagnostics\n");
    printf("  diag_pass                            Show diagnostics delta since selftest_pass\n");
    printf("  diag_dpc                             Show diagnostics delta since selftest_dpc\n");
    printf("  quit                                 Exit the tool\n");
}

void RunLegacyDemo(HANDLE device) {
    printf("=== Sphere Collision Demo ===\n");
    FCL_SPHERE_COLLISION_BUFFER buffer = {};
    buffer.SphereA.Center = {0.0f, 0.0f, 0.0f};
    buffer.SphereA.Radius = 0.5f;
    buffer.SphereB.Center = {0.8f, 0.0f, 0.0f};
    buffer.SphereB.Radius = 0.5f;
    if (SendIoctl(device, IOCTL_FCL_DEMO_SPHERE_COLLISION, &buffer, sizeof(buffer))) {
        printf("  Result: %s\n", buffer.Result.IsColliding ? "Intersecting" : "Separated");
    }
}

SceneObject* FindObject(SceneMap& objects, const std::string& name) {
    auto it = objects.find(name);
    if (it == objects.end()) {
        printf("Object not found: %s\n", name.c_str());
        return nullptr;
    }
    return &it->second;
}

bool ExecuteCommand(const std::vector<std::string>& tokens, HANDLE device, SceneMap& objects);

bool RunScript(const std::filesystem::path& scriptPath, HANDLE device, SceneMap& objects) {
    std::ifstream script(scriptPath);
    if (!script) {
        printf("Failed to open script file: %s\n", scriptPath.string().c_str());
        return true;
    }

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(script, line)) {
        ++lineNumber;
        std::string trimmed = line;
        const size_t first = trimmed.find_first_not_of(" \t\r");
        if (first == std::string::npos) {
            continue;
        }
        trimmed = trimmed.substr(first);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        auto scriptTokens = Tokenize(trimmed);
        if (scriptTokens.empty()) {
            continue;
        }
        printf("[script:%s:%zu] %s\n",
               scriptPath.string().c_str(),
               lineNumber,
               trimmed.c_str());
        if (!ExecuteCommand(scriptTokens, device, objects)) {
            return false;
        }
    }
    return true;
}

bool ExecuteCommand(const std::vector<std::string>& tokens, HANDLE device, SceneMap& objects) {
    if (tokens.empty()) {
        return true;
    }
    const std::string& cmd = tokens[0];
    if (cmd == "quit" || cmd == "exit") {
        return false;
    } else if (cmd == "help") {
        PrintHelp();
    } else if (cmd == "list") {
        ListObjects(objects);
    } else if (cmd == "demo") {
        RunLegacyDemo(device);
    } else if (cmd == "diag") {
        FCL_DIAGNOSTICS_RESPONSE diag = {};
        if (!SendIoctl(device, IOCTL_FCL_QUERY_DIAGNOSTICS, &diag, sizeof(diag))) {
            printf("  [FAIL] Diagnostics IOCTL failed.\n");
            return true;
        }

        printf("Kernel detection timing diagnostics (microseconds aggregated):\n");
        PrintTimingStats("Collision", diag.Collision);
        PrintTimingStats("Distance", diag.Distance);
        PrintTimingStats("ContinuousCollision", diag.ContinuousCollision);
    } else if (cmd == "diag_pass") {
        return PrintDiagPass(device);
    } else if (cmd == "diag_dpc") {
        return PrintDiagDpc(device);
    } else if (cmd == "selftest") {
        if (tokens.size() == 2) {
            return RunSelfTestScenario(device, tokens[1]);
        }

        printf("Running ping...\n");
        FCL_PING_RESPONSE ping = {};
        if (!SendIoctl(device, IOCTL_FCL_PING, &ping, sizeof(ping))) {
            printf("  [FAIL] Ping IOCTL failed.\n");
            return true;
        }
        printf("  [OK]   Version=%u.%u.%u.%u initialized=%u initializing=%u lastError=0x%08X\n",
               ping.Version.Major,
               ping.Version.Minor,
               ping.Version.Patch,
               ping.Version.Build,
               ping.IsInitialized,
               ping.IsInitializing,
               ping.LastError);

        printf("Running kernel self-test (IOCTL_FCL_SELF_TEST)...\n");
        FCL_SELF_TEST_RESULT self = {};
        if (!SendIoctl(device, IOCTL_FCL_SELF_TEST, &self, sizeof(self))) {
            printf("  [FAIL] Self-test IOCTL failed.\n");
            return true;
        }

        auto printStatus = [](const char* name, int32_t status) {
            if (status == 0) {
                printf("  [OK]   %-16s (0x%08X)\n", name, status);
            } else {
                printf("  [FAIL] %-16s (0x%08X)\n", name, status);
            }
        };

        printStatus("Initialize", self.InitializeStatus);
        printStatus("Collision", self.CollisionStatus);
        printStatus("Broadphase", self.BroadphaseStatus);
        printStatus("MeshGjk", self.MeshGjkStatus);
        printStatus("CCD", self.ContinuousCollisionStatus);

        const bool overallOk = (self.OverallStatus == 0) && (self.Passed != 0);
        printf("Summary: %s (overall=0x%08X, passed=%u)\n",
               overallOk ? "PASSED" : "FAILED",
               self.OverallStatus,
               self.Passed);
        printf("  CollisionDetected=%u BroadphasePairs=%u Distance=%.4f\n",
               self.CollisionDetected,
               self.BroadphasePairCount,
               self.DistanceValue);
    } else if (cmd == "selftest_pass") {
        return SelfTestPass(device);
    } else if (cmd == "selftest_dpc") {
        return SelfTestDpc(device);
    } else if (cmd == "run") {
        if (tokens.size() != 2) {
            printf("Usage: run <script_path>\n");
        } else {
            std::filesystem::path path = std::filesystem::u8path(tokens[1]);
            if (!RunScript(path, device, objects)) {
                return false;
            }
        }
    } else if (cmd == "load") {
        if (tokens.size() < 3) {
            printf("Usage: load <name> <obj_path>\n");
            return true;
        }
        if (objects.find(tokens[1]) != objects.end()) {
            printf("Name already exists: %s\n", tokens[1].c_str());
            return true;
        }
        std::filesystem::path path = std::filesystem::u8path(tokens[2]);
        std::vector<FCL_VECTOR3> vertices;
        std::vector<uint32_t> indices;
        std::string error;
        if (!LoadObjMesh(path, vertices, indices, error)) {
            printf("%s\n", error.c_str());
            return true;
        }
        FCL_GEOMETRY_HANDLE handle = {};
        if (!CreateMesh(device, vertices, indices, handle)) {
            return true;
        }
        SceneObject obj;
        obj.Name = tokens[1];
        obj.Handle = handle;
        obj.Transform = IdentityTransform();
        obj.Type = "mesh";
        obj.VertexCount = vertices.size();
        obj.IndexCount = indices.size();
        objects.emplace(obj.Name, obj);
        printf("Mesh %s created (handle=%llu, vertices=%zu, indices=%zu)\n",
               obj.Name.c_str(),
               static_cast<unsigned long long>(handle.Value),
               vertices.size(),
               indices.size());
    } else if (cmd == "sphere") {
        if (tokens.size() != 3 && tokens.size() != 6) {
            printf("Usage: sphere <name> <radius> [x y z]\n");
            return true;
        }
        if (objects.find(tokens[1]) != objects.end()) {
            printf("Name already exists: %s\n", tokens[1].c_str());
            return true;
        }
        float radius = 0.0f;
        if (!ParseFloat(tokens[2], radius) || radius <= 0.0f) {
            printf("Radius must be > 0.\n");
            return true;
        }
        FCL_VECTOR3 center = {0.0f, 0.0f, 0.0f};
        if (tokens.size() == 6) {
            if (!ParseFloat(tokens[3], center.X) ||
                !ParseFloat(tokens[4], center.Y) ||
                !ParseFloat(tokens[5], center.Z)) {
                printf("Position is invalid.\n");
                return true;
            }
        }
        FCL_GEOMETRY_HANDLE handle = {};
        if (!CreateSphere(device, radius, center, handle)) {
            return true;
        }
        SceneObject obj;
        obj.Name = tokens[1];
        obj.Handle = handle;
        obj.Transform = IdentityTransform();
        obj.Transform.Translation = center;
        obj.Type = "sphere";
        objects.emplace(obj.Name, obj);
        printf("Sphere %s created (handle=%llu)\n",
               obj.Name.c_str(),
               static_cast<unsigned long long>(handle.Value));
    } else if (cmd == "move") {
        if (tokens.size() != 4) {
            printf("Usage: move <name> <x> <y> <z>\n");
            return true;
        }
        auto* obj = FindObject(objects, tokens[1]);
        if (!obj) {
            return true;
        }
        if (!ParseFloat(tokens[2], obj->Transform.Translation.X) ||
            !ParseFloat(tokens[3], obj->Transform.Translation.Y) ||
            !ParseFloat(tokens[4], obj->Transform.Translation.Z)) {
            printf("Position is invalid.\n");
            return true;
        }
        printf("%s -> position (%.3f, %.3f, %.3f)\n",
               obj->Name.c_str(),
               obj->Transform.Translation.X,
               obj->Transform.Translation.Y,
               obj->Transform.Translation.Z);
    } else if (cmd == "collide") {
        if (tokens.size() != 3) {
            printf("Usage: collide <nameA> <nameB>\n");
            return true;
        }
        auto* a = FindObject(objects, tokens[1]);
        auto* b = FindObject(objects, tokens[2]);
        if (!a || !b) {
            return true;
        }
        FCL_COLLISION_RESULT result = {};
        if (QueryCollision(device, *a, *b, result)) {
            PrintCollisionInfo(*a, *b, result);
        }
    } else if (cmd == "distance") {
        if (tokens.size() != 3) {
            printf("Usage: distance <nameA> <nameB>\n");
            return true;
        }
        auto* a = FindObject(objects, tokens[1]);
        auto* b = FindObject(objects, tokens[2]);
        if (!a || !b) {
            return true;
        }
        FCL_DISTANCE_OUTPUT output = {};
        if (QueryDistance(device, *a, *b, output)) {
            PrintDistanceInfo(*a, *b, output);
        }
    } else if (cmd == "simulate") {
        if (tokens.size() != 8) {
            printf("Usage: simulate <mov> <static> <dx> <dy> <dz> <steps> <interval_ms>\n");
            return true;
        }
        auto* mover = FindObject(objects, tokens[1]);
        auto* target = FindObject(objects, tokens[2]);
        if (!mover || !target) {
            return true;
        }
        FCL_VECTOR3 delta = {};
        if (!ParseFloat(tokens[3], delta.X) ||
            !ParseFloat(tokens[4], delta.Y) ||
            !ParseFloat(tokens[5], delta.Z)) {
            printf("Delta vector is invalid.\n");
            return true;
        }
        int steps = 0;
        int intervalMs = 0;
        if (!ParseInt(tokens[6], steps) || steps <= 0 ||
            !ParseInt(tokens[7], intervalMs) || intervalMs < 0) {
            printf("Steps or interval is invalid.\n");
            return true;
        }
        for (int i = 0; i < steps; ++i) {
            mover->Transform.Translation.X += delta.X;
            mover->Transform.Translation.Y += delta.Y;
            mover->Transform.Translation.Z += delta.Z;
            FCL_COLLISION_RESULT result = {};
            if (QueryCollision(device, *mover, *target, result)) {
                printf("[Step %d] mover=(%.3f, %.3f, %.3f)\n",
                       i + 1,
                       mover->Transform.Translation.X,
                       mover->Transform.Translation.Y,
                       mover->Transform.Translation.Z);
                PrintCollisionInfo(*mover, *target, result);
            } else {
                break;
            }
            if (intervalMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            }
        }
    } else if (cmd == "periodic") {
        if (tokens.size() != 4) {
            printf("Usage: periodic <mov> <static> <period_us>\n");
            return true;
        }
        auto* mover = FindObject(objects, tokens[1]);
        auto* target = FindObject(objects, tokens[2]);
        if (!mover || !target) {
            return true;
        }
        int periodUsSigned = 0;
        if (!ParseInt(tokens[3], periodUsSigned) || periodUsSigned <= 0) {
            printf("Period must be a positive integer microseconds value.\n");
            return true;
        }
        const uint32_t periodUs = static_cast<uint32_t>(periodUsSigned);
        (void)StartPeriodicCollision(device, *mover, *target, periodUs);
    } else if (cmd == "periodic_stop") {
        (void)StopPeriodicCollision(device);
    } else if (cmd == "ccd") {
        if (tokens.size() != 7) {
            printf("Usage: ccd <mov> <static> <dx> <dy> <dz>\n");
            return true;
        }
        auto* mover = FindObject(objects, tokens[1]);
        auto* target = FindObject(objects, tokens[2]);
        if (!mover || !target) {
            return true;
        }
        FCL_VECTOR3 delta = {};
        if (!ParseFloat(tokens[3], delta.X) ||
            !ParseFloat(tokens[4], delta.Y) ||
            !ParseFloat(tokens[5], delta.Z)) {
            printf("Delta vector is invalid.\n");
            return true;
        }
        double toi = 0.0;
        if (RunConvexCcd(device, *mover, *target, delta, toi)) {
            printf("CCD detected intersection. TOI=%.4f\n", toi);
        } else {
            printf("CCD did not detect a collision.\n");
        }
    } else if (cmd == "destroy") {
        if (tokens.size() != 2) {
            printf("Usage: destroy <name>\n");
            return true;
        }
        auto it = objects.find(tokens[1]);
        if (it == objects.end()) {
            printf("Object not found: %s\n", tokens[1].c_str());
            return true;
        }
        DestroyGeometry(device, it->second.Handle);
        printf("Object %s deleted.\n", tokens[1].c_str());
        objects.erase(it);
    } else {
        printf("Unknown command: %s\n", cmd.c_str());
    }
    return true;
}

int main() {
    HANDLE device = CreateFileW(LR"(\\.\FclMusa)",
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (device == INVALID_HANDLE_VALUE) {
        printf("Failed to open \\\\.\\FclMusa (error=0x%08lX)\n", GetLastError());
        return 1;
    }

    SceneMap objects;
    PrintHelp();

    bool running = true;
    std::string line;
    while (running) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        auto tokens = Tokenize(line);
        running = ExecuteCommand(tokens, device, objects);
    }

    for (auto& [name, obj] : objects) {
        DestroyGeometry(device, obj.Handle);
    }
    CloseHandle(device);
    return 0;
}
