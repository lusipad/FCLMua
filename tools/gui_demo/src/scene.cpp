#include "scene.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

XMMATRIX SceneObject::GetRotationMatrix() const
{
    return XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
}

XMMATRIX SceneObject::GetWorldMatrix() const
{
    XMMATRIX scaleMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX rotMatrix = GetRotationMatrix();
    XMMATRIX transMatrix = XMMatrixTranslation(position.x, position.y, position.z);
    return scaleMatrix * rotMatrix * transMatrix;
}

Scene::Scene(Renderer* renderer, FclDriver* driver)
    : m_renderer(renderer)
    , m_driver(driver)
    , m_selectedObjectIndex(static_cast<size_t>(-1))
    , m_isDragging(false)
    , m_isPanning(false)
    , m_isRotatingCamera(false)
    , m_lastMouseX(0)
    , m_lastMouseY(0)
{
}

Scene::~Scene()
{
    ClearAllObjects();
}

void Scene::Initialize()
{
    // Initialize camera
    m_camera.SetTarget(XMFLOAT3(0, 0, 0));
    m_camera.SetDistance(15.0f);

    // Add some default objects for demo
    AddSphere("Sphere 1", XMFLOAT3(-3, 2, 0), 1.0f);
    AddSphere("Sphere 2", XMFLOAT3(3, 2, 0), 1.5f);
    AddBox("Box 1", XMFLOAT3(0, 2, -3), XMFLOAT3(1, 1, 1));

    // Set some colors
    if (m_objects.size() > 0)
        m_objects[0]->color = XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f); // Red
    if (m_objects.size() > 1)
        m_objects[1]->color = XMFLOAT4(0.3f, 0.7f, 1.0f, 1.0f); // Blue
    if (m_objects.size() > 2)
        m_objects[2]->color = XMFLOAT4(0.3f, 1.0f, 0.3f, 1.0f); // Green
}

void Scene::Update(float deltaTime)
{
    // Update logic (animation, physics, etc.)
    // For now, just detect collisions
    DetectCollisions();
}

void Scene::Render()
{
    // Set up view and projection matrices
    float aspectRatio = static_cast<float>(m_renderer->GetDevice() ? 16.0f / 9.0f : 1.0f);
    XMMATRIX view = m_camera.GetViewMatrix();
    XMMATRIX proj = m_camera.GetProjectionMatrix(aspectRatio);
    m_renderer->SetViewProjection(view, proj);

    // Render grid
    RenderGrid();

    // Render objects
    RenderObjects();

    // Render gizmo for selected object
    if (GetSelectedObject())
    {
        RenderGizmo();
    }
}

void Scene::HandleInput(Window* window, float deltaTime)
{
    int mouseX, mouseY, mouseDX, mouseDY;
    window->GetMousePosition(mouseX, mouseY);
    window->GetMouseDelta(mouseDX, mouseDY);

    // Camera rotation (Right mouse button)
    if (window->IsMouseButtonDown(1))
    {
        if (!m_isRotatingCamera)
        {
            m_isRotatingCamera = true;
        }
        else
        {
            float sensitivity = 0.005f;
            m_camera.Rotate(-mouseDX * sensitivity, -mouseDY * sensitivity);
        }
    }
    else
    {
        m_isRotatingCamera = false;
    }

    // Camera pan (Middle mouse button)
    if (window->IsMouseButtonDown(2))
    {
        if (!m_isPanning)
        {
            m_isPanning = true;
        }
        else
        {
            float sensitivity = 0.01f * m_camera.GetDistance();
            m_camera.Pan(-mouseDX * sensitivity, mouseDY * sensitivity);
        }
    }
    else
    {
        m_isPanning = false;
    }

    // Camera zoom (Mouse wheel)
    int mouseWheel = window->GetMouseWheel();
    if (mouseWheel != 0)
    {
        float zoomAmount = mouseWheel / 120.0f;
        m_camera.Zoom(-zoomAmount * m_camera.GetDistance() * 0.1f);
    }

    // Object dragging (Left mouse button)
    SceneObject* selected = GetSelectedObject();
    if (selected && window->IsMouseButtonDown(0))
    {
        if (!m_isDragging)
        {
            m_isDragging = true;
        }
        else
        {
            // Simple XZ plane dragging
            float sensitivity = 0.01f * m_camera.GetDistance();
            selected->position.x -= mouseDX * sensitivity;
            selected->position.z += mouseDY * sensitivity;
        }
    }
    else
    {
        m_isDragging = false;
    }

    // Keyboard controls
    if (window->IsKeyDown('W'))
    {
        if (selected)
            selected->position.y += 2.0f * deltaTime;
    }
    if (window->IsKeyDown('S'))
    {
        if (selected)
            selected->position.y -= 2.0f * deltaTime;
    }
    if (window->IsKeyDown('Q'))
    {
        if (selected)
            selected->rotation.y -= 2.0f * deltaTime;
    }
    if (window->IsKeyDown('E'))
    {
        if (selected)
            selected->rotation.y += 2.0f * deltaTime;
    }

    // Selection (1-9 keys)
    for (int i = 0; i < 9 && i < static_cast<int>(m_objects.size()); ++i)
    {
        if (window->IsKeyDown('1' + i))
        {
            SelectObject(i);
        }
    }

    // Deselect (Escape)
    if (window->IsKeyDown(VK_ESCAPE))
    {
        DeselectAll();
    }

    // Create new objects (C/B/M keys with Ctrl)
    static bool ctrlCPressed = false;
    static bool ctrlBPressed = false;
    static bool ctrlMPressed = false;

    bool ctrlDown = window->IsKeyDown(VK_CONTROL);

    // Create Sphere (Ctrl+C)
    if (ctrlDown && window->IsKeyDown('C'))
    {
        if (!ctrlCPressed)
        {
            XMFLOAT3 cameraTarget = m_camera.GetTarget();
            float radius = 1.0f;
            std::string name = "Sphere " + std::to_string(m_objects.size() + 1);
            AddSphere(name, XMFLOAT3(cameraTarget.x, cameraTarget.y + 2.0f, cameraTarget.z), radius);
            SelectObject(m_objects.size() - 1);
            ctrlCPressed = true;
        }
    }
    else
    {
        ctrlCPressed = false;
    }

    // Create Box (Ctrl+B)
    if (ctrlDown && window->IsKeyDown('B'))
    {
        if (!ctrlBPressed)
        {
            XMFLOAT3 cameraTarget = m_camera.GetTarget();
            XMFLOAT3 extents(1.0f, 1.0f, 1.0f);
            std::string name = "Box " + std::to_string(m_objects.size() + 1);
            AddBox(name, XMFLOAT3(cameraTarget.x, cameraTarget.y + 2.0f, cameraTarget.z), extents);
            SelectObject(m_objects.size() - 1);
            ctrlBPressed = true;
        }
    }
    else
    {
        ctrlBPressed = false;
    }

    // Delete selected object (Delete key)
    static bool deletePressed = false;
    if (window->IsKeyDown(VK_DELETE))
    {
        if (!deletePressed && m_selectedObjectIndex < m_objects.size())
        {
            DeleteObject(m_selectedObjectIndex);
            m_selectedObjectIndex = static_cast<size_t>(-1);
            deletePressed = true;
        }
    }
    else
    {
        deletePressed = false;
    }
}

void Scene::RenderObjects()
{
    for (const auto& obj : m_objects)
    {
        // Determine color (highlight if selected or colliding)
        XMFLOAT4 color = obj->color;
        if (obj->isSelected)
        {
            color = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow for selected
        }
        else if (obj->isColliding)
        {
            color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f); // Red for colliding
        }

        switch (obj->type)
        {
        case GeometryType::Sphere:
        {
            XMMATRIX world = XMMatrixScaling(obj->data.sphere.radius, obj->data.sphere.radius, obj->data.sphere.radius) *
                             XMMatrixTranslation(obj->position.x, obj->position.y, obj->position.z);
            m_renderer->DrawSphere(obj->position, obj->data.sphere.radius, color);
            break;
        }
        case GeometryType::Box:
        {
            m_renderer->DrawBox(obj->position, obj->data.box.extents, obj->GetRotationMatrix(), color);
            break;
        }
        case GeometryType::Mesh:
        {
            if (obj->data.customMesh.mesh)
            {
                m_renderer->DrawMesh(*obj->data.customMesh.mesh, obj->GetWorldMatrix(), color);
            }
            break;
        }
        }
    }
}

void Scene::RenderGrid()
{
    // Simple grid rendering
    const float gridSize = 20.0f;
    const int gridDivisions = 20;
    const float gridStep = gridSize / gridDivisions;
    const XMFLOAT4 gridColor(0.3f, 0.3f, 0.3f, 1.0f);

    for (int i = 0; i <= gridDivisions; ++i)
    {
        float pos = -gridSize / 2.0f + i * gridStep;

        // Lines along X axis
        m_renderer->DrawLine(
            XMFLOAT3(-gridSize / 2.0f, 0, pos),
            XMFLOAT3(gridSize / 2.0f, 0, pos),
            gridColor
        );

        // Lines along Z axis
        m_renderer->DrawLine(
            XMFLOAT3(pos, 0, -gridSize / 2.0f),
            XMFLOAT3(pos, 0, gridSize / 2.0f),
            gridColor
        );
    }
}

void Scene::RenderGizmo()
{
    SceneObject* obj = GetSelectedObject();
    if (!obj)
        return;

    // Render simple axis gizmo
    const float gizmoSize = 2.0f;
    XMFLOAT3 pos = obj->position;

    // X axis (red)
    m_renderer->DrawLine(pos, XMFLOAT3(pos.x + gizmoSize, pos.y, pos.z), XMFLOAT4(1, 0, 0, 1));
    // Y axis (green)
    m_renderer->DrawLine(pos, XMFLOAT3(pos.x, pos.y + gizmoSize, pos.z), XMFLOAT4(0, 1, 0, 1));
    // Z axis (blue)
    m_renderer->DrawLine(pos, XMFLOAT3(pos.x, pos.y, pos.z + gizmoSize), XMFLOAT4(0, 0, 1, 1));
}

void Scene::AddSphere(const std::string& name, const XMFLOAT3& position, float radius)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name;
    obj->type = GeometryType::Sphere;
    obj->position = position;
    obj->data.sphere.radius = radius;

    // Create FCL geometry if driver is connected
    if (m_driver && m_driver->IsConnected())
    {
        obj->fclHandle = m_driver->CreateSphere(position, radius);
    }

    m_objects.push_back(std::move(obj));
}

void Scene::AddBox(const std::string& name, const XMFLOAT3& position, const XMFLOAT3& extents)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name;
    obj->type = GeometryType::Box;
    obj->position = position;
    obj->data.box.extents = extents;

    // For boxes, we could create an OBB in FCL, but we'll use a mesh approximation
    // TODO: Implement OBB creation in driver

    m_objects.push_back(std::move(obj));
}

void Scene::AddMesh(const std::string& name, const XMFLOAT3& position,
                    const std::vector<XMFLOAT3>& vertices,
                    const std::vector<uint32_t>& indices)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name;
    obj->type = GeometryType::Mesh;
    obj->position = position;

    // Create mesh
    Mesh mesh = Renderer::CreateMeshFromData(vertices, indices);
    m_renderer->UploadMesh(mesh);
    obj->data.customMesh.mesh = new Mesh(std::move(mesh));

    // Create FCL geometry if driver is connected
    if (m_driver && m_driver->IsConnected())
    {
        obj->fclHandle = m_driver->CreateMesh(vertices, indices);
    }

    m_objects.push_back(std::move(obj));
}

void Scene::DeleteObject(size_t index)
{
    if (index >= m_objects.size())
        return;

    // Destroy FCL geometry
    if (m_driver && m_driver->IsConnected())
    {
        m_driver->DestroyGeometry(m_objects[index]->fclHandle);
    }

    // Free mesh if it's a custom mesh
    if (m_objects[index]->type == GeometryType::Mesh && m_objects[index]->data.customMesh.mesh)
    {
        delete m_objects[index]->data.customMesh.mesh;
    }

    m_objects.erase(m_objects.begin() + index);

    // Update selection
    if (m_selectedObjectIndex == index)
    {
        m_selectedObjectIndex = static_cast<size_t>(-1);
    }
    else if (m_selectedObjectIndex > index)
    {
        m_selectedObjectIndex--;
    }
}

void Scene::ClearAllObjects()
{
    for (auto& obj : m_objects)
    {
        if (m_driver && m_driver->IsConnected())
        {
            m_driver->DestroyGeometry(obj->fclHandle);
        }

        if (obj->type == GeometryType::Mesh && obj->data.customMesh.mesh)
        {
            delete obj->data.customMesh.mesh;
        }
    }

    m_objects.clear();
    m_selectedObjectIndex = static_cast<size_t>(-1);
}

void Scene::SelectObject(size_t index)
{
    DeselectAll();
    if (index < m_objects.size())
    {
        m_objects[index]->isSelected = true;
        m_selectedObjectIndex = index;
    }
}

void Scene::DeselectAll()
{
    for (auto& obj : m_objects)
    {
        obj->isSelected = false;
    }
    m_selectedObjectIndex = static_cast<size_t>(-1);
}

SceneObject* Scene::GetSelectedObject()
{
    if (m_selectedObjectIndex < m_objects.size())
    {
        return m_objects[m_selectedObjectIndex].get();
    }
    return nullptr;
}

SceneObject* Scene::GetObject(size_t index)
{
    if (index < m_objects.size())
    {
        return m_objects[index].get();
    }
    return nullptr;
}

void Scene::DetectCollisions()
{
    if (!m_driver || !m_driver->IsConnected())
        return;

    // Reset collision flags
    for (auto& obj : m_objects)
    {
        obj->isColliding = false;
    }

    // Check all pairs
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        for (size_t j = i + 1; j < m_objects.size(); ++j)
        {
            auto& obj1 = m_objects[i];
            auto& obj2 = m_objects[j];

            if (obj1->fclHandle.Value == 0 || obj2->fclHandle.Value == 0)
                continue;

            // Create transforms
            FCL_TRANSFORM transform1 = FclDriver::CreateTransform(obj1->position, obj1->GetRotationMatrix());
            FCL_TRANSFORM transform2 = FclDriver::CreateTransform(obj2->position, obj2->GetRotationMatrix());

            // Query collision
            bool isColliding = false;
            FCL_CONTACT_INFO contactInfo;

            if (m_driver->QueryCollision(obj1->fclHandle, transform1, obj2->fclHandle, transform2,
                                         isColliding, contactInfo))
            {
                if (isColliding)
                {
                    obj1->isColliding = true;
                    obj2->isColliding = true;
                }
            }
        }
    }
}
