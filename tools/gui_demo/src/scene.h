#pragma once
#include <vector>
#include <memory>
#include <string>
#include <directxmath.h>
#include "renderer.h"
#include "camera.h"
#include "fcl_driver.h"
#include "window.h"

using namespace DirectX;

// Geometry types
enum class GeometryType
{
    Sphere,
    Box,
    Mesh
};

// Scene object
struct SceneObject
{
    std::string name;
    GeometryType type;
    XMFLOAT3 position;
    XMFLOAT3 rotation; // Euler angles
    XMFLOAT3 scale;
    XMFLOAT4 color;
    bool isSelected;
    bool isColliding;

    // Geometry-specific data
    union {
        struct {
            float radius;
        } sphere;
        struct {
            XMFLOAT3 extents;
        } box;
        struct {
            Mesh* mesh;
        } customMesh;
    } data;

    // FCL handle
    FCL_GEOMETRY_HANDLE fclHandle;

    SceneObject()
        : position(0, 0, 0)
        , rotation(0, 0, 0)
        , scale(1, 1, 1)
        , color(0.7f, 0.7f, 0.7f, 1.0f)
        , isSelected(false)
        , isColliding(false)
        , fclHandle{ 0 }
    {
        memset(&data, 0, sizeof(data));
    }

    XMMATRIX GetWorldMatrix() const;
    XMMATRIX GetRotationMatrix() const;
};

class Scene
{
public:
    Scene(Renderer* renderer, FclDriver* driver);
    ~Scene();

    void Initialize();
    void Update(float deltaTime);
    void Render();

    // Object management
    void AddSphere(const std::string& name, const XMFLOAT3& position, float radius);
    void AddBox(const std::string& name, const XMFLOAT3& position, const XMFLOAT3& extents);
    void AddMesh(const std::string& name, const XMFLOAT3& position,
                 const std::vector<XMFLOAT3>& vertices,
                 const std::vector<uint32_t>& indices);
    void DeleteObject(size_t index);
    void ClearAllObjects();

    // Selection and transformation
    void SelectObject(size_t index);
    void DeselectAll();
    SceneObject* GetSelectedObject();
    size_t GetSelectedObjectIndex() const { return m_selectedObjectIndex; }

    // Collision detection
    void DetectCollisions();

    // Accessors
    size_t GetObjectCount() const { return m_objects.size(); }
    SceneObject* GetObject(size_t index);
    Camera& GetCamera() { return m_camera; }

    // Input handling (called by main loop)
    void HandleInput(Window* window, float deltaTime);

private:
    void RenderObjects();
    void RenderGrid();
    void RenderGizmo();

    Renderer* m_renderer;
    FclDriver* m_driver;
    Camera m_camera;

    std::vector<std::unique_ptr<SceneObject>> m_objects;
    size_t m_selectedObjectIndex;

    // Input state
    bool m_isDragging;
    bool m_isPanning;
    bool m_isRotatingCamera;
    int m_lastMouseX;
    int m_lastMouseY;
};
