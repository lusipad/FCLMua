#pragma once
#include <vector>
#include <string>
#include <directxmath.h>

using namespace DirectX;

/// <summary>
/// Simple OBJ file loader for loading 3D vehicle models
/// Supports vertices, normals, texture coordinates, and faces
/// </summary>
class ObjLoader
{
public:
    struct MeshData
    {
        std::vector<XMFLOAT3> vertices;
        std::vector<uint32_t> indices;
        std::vector<XMFLOAT3> normals;
        std::vector<XMFLOAT2> texCoords;

        // Bounding box for collision detection
        XMFLOAT3 minBounds;
        XMFLOAT3 maxBounds;
    };

    /// <summary>
    /// Load an OBJ file from disk
    /// </summary>
    /// <param name="filePath">Path to the .obj file</param>
    /// <param name="outMesh">Output mesh data</param>
    /// <param name="scale">Optional scale factor (default 1.0)</param>
    /// <returns>True if successful, false otherwise</returns>
    static bool LoadFromFile(const std::string& filePath, MeshData& outMesh, float scale = 1.0f);

    /// <summary>
    /// Load an OBJ file from disk (wide string version for Windows)
    /// </summary>
    static bool LoadFromFile(const std::wstring& filePath, MeshData& outMesh, float scale = 1.0f);

private:
    static void CalculateBounds(MeshData& mesh);
    static void TriangulateQuad(const std::vector<int>& faceIndices,
                               std::vector<uint32_t>& outIndices);
};
