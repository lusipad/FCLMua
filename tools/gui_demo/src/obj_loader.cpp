#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "obj_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <locale>
#include <codecvt>

bool ObjLoader::LoadFromFile(const std::wstring& filePath, MeshData& outMesh, float scale)
{
    // Convert wide string to narrow string
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string narrowPath = converter.to_bytes(filePath);
    return LoadFromFile(narrowPath, outMesh, scale);
}

bool ObjLoader::LoadFromFile(const std::string& filePath, MeshData& outMesh, float scale)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    // Temporary storage for file data
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> texCoords;

    // Final mesh data
    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.normals.clear();
    outMesh.texCoords.clear();

    std::string line;
    while (std::getline(file, line))
    {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v")
        {
            // Vertex position
            XMFLOAT3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            pos.x *= scale;
            pos.y *= scale;
            pos.z *= scale;
            positions.push_back(pos);
        }
        else if (prefix == "vn")
        {
            // Vertex normal
            XMFLOAT3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (prefix == "vt")
        {
            // Texture coordinate
            XMFLOAT2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            texCoords.push_back(texCoord);
        }
        else if (prefix == "f")
        {
            // Face (can be triangles or quads)
            std::vector<int> vertexIndices;
            std::vector<int> texCoordIndices;
            std::vector<int> normalIndices;

            std::string vertexData;
            while (iss >> vertexData)
            {
                // Parse vertex data: v/vt/vn or v//vn or v/vt or v
                int vIdx = 0, vtIdx = 0, vnIdx = 0;

                size_t slash1 = vertexData.find('/');
                if (slash1 == std::string::npos)
                {
                    // Format: v
                    vIdx = std::stoi(vertexData);
                }
                else
                {
                    vIdx = std::stoi(vertexData.substr(0, slash1));

                    size_t slash2 = vertexData.find('/', slash1 + 1);
                    if (slash2 == std::string::npos)
                    {
                        // Format: v/vt
                        vtIdx = std::stoi(vertexData.substr(slash1 + 1));
                    }
                    else
                    {
                        // Format: v/vt/vn or v//vn
                        if (slash2 > slash1 + 1)
                        {
                            vtIdx = std::stoi(vertexData.substr(slash1 + 1, slash2 - slash1 - 1));
                        }
                        vnIdx = std::stoi(vertexData.substr(slash2 + 1));
                    }
                }

                // OBJ indices are 1-based, convert to 0-based
                if (vIdx > 0) vIdx--;
                else if (vIdx < 0) vIdx = static_cast<int>(positions.size()) + vIdx;

                if (vtIdx > 0) vtIdx--;
                else if (vtIdx < 0) vtIdx = static_cast<int>(texCoords.size()) + vtIdx;

                if (vnIdx > 0) vnIdx--;
                else if (vnIdx < 0) vnIdx = static_cast<int>(normals.size()) + vnIdx;

                vertexIndices.push_back(vIdx);
                texCoordIndices.push_back(vtIdx);
                normalIndices.push_back(vnIdx);
            }

            // Triangulate face (handle triangles and quads)
            if (vertexIndices.size() >= 3)
            {
                // For each vertex in the face, create a unique vertex entry
                for (size_t i = 0; i < vertexIndices.size(); ++i)
                {
                    int vIdx = vertexIndices[i];
                    int vtIdx = texCoordIndices[i];
                    int vnIdx = normalIndices[i];

                    // Add vertex data
                    if (vIdx >= 0 && vIdx < static_cast<int>(positions.size()))
                    {
                        outMesh.vertices.push_back(positions[vIdx]);
                    }

                    if (vnIdx >= 0 && vnIdx < static_cast<int>(normals.size()))
                    {
                        outMesh.normals.push_back(normals[vnIdx]);
                    }
                    else
                    {
                        outMesh.normals.push_back(XMFLOAT3(0, 1, 0)); // Default normal
                    }

                    if (vtIdx >= 0 && vtIdx < static_cast<int>(texCoords.size()))
                    {
                        outMesh.texCoords.push_back(texCoords[vtIdx]);
                    }
                    else
                    {
                        outMesh.texCoords.push_back(XMFLOAT2(0, 0)); // Default tex coord
                    }
                }

                // Generate indices for triangulation
                size_t baseIndex = outMesh.vertices.size() - vertexIndices.size();

                if (vertexIndices.size() == 3)
                {
                    // Triangle
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 0));
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 1));
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 2));
                }
                else if (vertexIndices.size() == 4)
                {
                    // Quad - split into two triangles
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 0));
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 1));
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 2));

                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 0));
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 2));
                    outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 3));
                }
                else
                {
                    // Polygon - simple fan triangulation
                    for (size_t i = 1; i < vertexIndices.size() - 1; ++i)
                    {
                        outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + 0));
                        outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + i));
                        outMesh.indices.push_back(static_cast<uint32_t>(baseIndex + i + 1));
                    }
                }
            }
        }
    }

    file.close();

    // Calculate bounding box
    CalculateBounds(outMesh);

    return !outMesh.vertices.empty();
}

void ObjLoader::CalculateBounds(MeshData& mesh)
{
    if (mesh.vertices.empty())
    {
        mesh.minBounds = XMFLOAT3(0, 0, 0);
        mesh.maxBounds = XMFLOAT3(0, 0, 0);
        return;
    }

    mesh.minBounds = mesh.vertices[0];
    mesh.maxBounds = mesh.vertices[0];

    for (const auto& vertex : mesh.vertices)
    {
        mesh.minBounds.x = std::min(mesh.minBounds.x, vertex.x);
        mesh.minBounds.y = std::min(mesh.minBounds.y, vertex.y);
        mesh.minBounds.z = std::min(mesh.minBounds.z, vertex.z);

        mesh.maxBounds.x = std::max(mesh.maxBounds.x, vertex.x);
        mesh.maxBounds.y = std::max(mesh.maxBounds.y, vertex.y);
        mesh.maxBounds.z = std::max(mesh.maxBounds.z, vertex.z);
    }
}

void ObjLoader::TriangulateQuad(const std::vector<int>& faceIndices,
                                std::vector<uint32_t>& outIndices)
{
    if (faceIndices.size() < 3)
        return;

    if (faceIndices.size() == 3)
    {
        // Already a triangle
        outIndices.push_back(faceIndices[0]);
        outIndices.push_back(faceIndices[1]);
        outIndices.push_back(faceIndices[2]);
    }
    else if (faceIndices.size() == 4)
    {
        // Quad - split into two triangles
        outIndices.push_back(faceIndices[0]);
        outIndices.push_back(faceIndices[1]);
        outIndices.push_back(faceIndices[2]);

        outIndices.push_back(faceIndices[0]);
        outIndices.push_back(faceIndices[2]);
        outIndices.push_back(faceIndices[3]);
    }
    else
    {
        // Polygon - simple fan triangulation
        for (size_t i = 1; i < faceIndices.size() - 1; ++i)
        {
            outIndices.push_back(faceIndices[0]);
            outIndices.push_back(faceIndices[i]);
            outIndices.push_back(faceIndices[i + 1]);
        }
    }
}
