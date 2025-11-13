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

#define IOCTL_FCL_PING                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_SELF_TEST            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_COLLISION      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_QUERY_DISTANCE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_SPHERE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_DESTROY_GEOMETRY     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CREATE_MESH          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x814, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_CONVEX_CCD           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x815, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_FCL_DEMO_SPHERE_COLLISION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

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

struct SceneObject {
    std::string Name;
    FCL_GEOMETRY_HANDLE Handle{};
    FCL_TRANSFORM Transform{};
    std::string Type;
    size_t VertexCount = 0;
    size_t IndexCount = 0;
};

using SceneMap = std::unordered_map<std::string, SceneObject>;

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
    printf("  destroy <name>                       Destroy a geometry\n");
    printf("  list                                 List registered geometries\n");
    printf("  demo                                 Run legacy sphere demo\n");
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

    printf("=== CCD Demo ===\n");
    FCL_GEOMETRY_HANDLE sphereA = {};
    FCL_GEOMETRY_HANDLE sphereB = {};
    if (!CreateSphere(device, 0.5f, {0.0f, 0.0f, 0.0f}, sphereA) ||
        !CreateSphere(device, 0.5f, {2.0f, 0.0f, 0.0f}, sphereB)) {
        DestroyGeometry(device, sphereA);
        DestroyGeometry(device, sphereB);
        return;
    }

    FCL_CONVEX_CCD_BUFFER ccd = {};
    ccd.Object1 = sphereA;
    ccd.Object2 = sphereB;
    ccd.Motion1.Start = IdentityTransform();
    ccd.Motion1.End = IdentityTransform();
    ccd.Motion1.End.Translation.X = 2.0f;
    ccd.Motion2.Start = IdentityTransform();
    ccd.Motion2.End = IdentityTransform();
    if (SendIoctl(device, IOCTL_FCL_CONVEX_CCD, &ccd, sizeof(ccd))) {
        printf("  CCD: %s, TOI=%.4f\n",
               ccd.Result.Intersecting ? "Intersecting" : "Separated",
               ccd.Result.TimeOfImpact);
    }
    DestroyGeometry(device, sphereA);
    DestroyGeometry(device, sphereB);
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
