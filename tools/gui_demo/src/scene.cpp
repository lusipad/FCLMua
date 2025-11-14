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
    , m_sceneMode(SceneMode::Default)
    , m_simulationSpeed(1.0f)
    , m_isDragging(false)
    , m_isPanning(false)
    , m_isRotatingCamera(false)
    , m_lastMouseX(0)
    , m_lastMouseY(0)
    , m_isCreatingAsteroid(false)
    , m_asteroidVelocity(0, 0, 0)
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

    // Initialize default scene
    InitializeDefaultScene();
}

void Scene::InitializeDefaultScene()
{
    ClearAllObjects();

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

void Scene::InitializeSolarSystem()
{
    ClearAllObjects();

    // Solar system data (relative sizes and orbital radii)
    // Sizes are scaled down for visibility
    // Orbital radii are compressed for better viewing
    const XMFLOAT3 sunCenter(0, 0, 0);

    // Sun (static, at center)
    auto sun = std::make_unique<SceneObject>();
    sun->name = "Sun";
    sun->type = GeometryType::Sphere;
    sun->position = sunCenter;
    sun->data.sphere.radius = 3.0f;
    sun->color = XMFLOAT4(1.0f, 0.9f, 0.2f, 1.0f); // Yellow
    sun->isOrbiting = false;
    if (m_driver && m_driver->IsConnected())
    {
        sun->fclHandle = m_driver->CreateSphere(sun->position, sun->data.sphere.radius);
    }
    m_objects.push_back(std::move(sun));

    // Planet data: {name, radius, orbital_radius, orbital_speed, color}
    struct PlanetData {
        const char* name;
        float radius;
        float orbitalRadius;
        float orbitalSpeed;  // radians per second (base speed)
        XMFLOAT4 color;
    };

    PlanetData planets[] = {
        // Inner planets
        {"Mercury", 0.15f, 5.0f,  0.8f, XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f)},  // Gray
        {"Venus",   0.35f, 7.0f,  0.6f, XMFLOAT4(0.9f, 0.7f, 0.4f, 1.0f)},  // Orange-ish
        {"Earth",   0.35f, 9.0f,  0.5f, XMFLOAT4(0.2f, 0.4f, 0.8f, 1.0f)},  // Blue
        {"Mars",    0.20f, 11.0f, 0.4f, XMFLOAT4(0.8f, 0.3f, 0.2f, 1.0f)},  // Red
        // Outer planets (gas giants)
        {"Jupiter", 1.0f,  15.0f, 0.25f, XMFLOAT4(0.8f, 0.6f, 0.4f, 1.0f)}, // Tan
        {"Saturn",  0.9f,  19.0f, 0.20f, XMFLOAT4(0.9f, 0.8f, 0.6f, 1.0f)}, // Pale yellow
        {"Uranus",  0.5f,  23.0f, 0.15f, XMFLOAT4(0.5f, 0.8f, 0.8f, 1.0f)}, // Cyan
        {"Neptune", 0.5f,  27.0f, 0.12f, XMFLOAT4(0.3f, 0.4f, 0.9f, 1.0f)}, // Deep blue
        // Dwarf planet
        {"Pluto",   0.10f, 31.0f, 0.08f, XMFLOAT4(0.8f, 0.7f, 0.6f, 1.0f)}  // Light brown
    };

    // Create all planets
    for (int i = 0; i < 9; ++i)
    {
        auto planet = std::make_unique<SceneObject>();
        planet->name = planets[i].name;
        planet->type = GeometryType::Sphere;
        planet->data.sphere.radius = planets[i].radius;
        planet->color = planets[i].color;

        // Set up orbital motion
        planet->isOrbiting = true;
        planet->orbitalRadius = planets[i].orbitalRadius;
        planet->orbitalSpeed = planets[i].orbitalSpeed;
        planet->orbitCenter = sunCenter;
        planet->currentAngle = static_cast<float>(i) * (2.0f * static_cast<float>(M_PI) / 9.0f); // Spread planets around

        // Calculate initial position based on angle
        planet->position.x = sunCenter.x + planet->orbitalRadius * cos(planet->currentAngle);
        planet->position.y = sunCenter.y;
        planet->position.z = sunCenter.z + planet->orbitalRadius * sin(planet->currentAngle);

        // Create FCL geometry
        if (m_driver && m_driver->IsConnected())
        {
            planet->fclHandle = m_driver->CreateSphere(planet->position, planet->data.sphere.radius);
        }

        m_objects.push_back(std::move(planet));
    }

    // Adjust camera for solar system view
    m_camera.SetTarget(sunCenter);
    m_camera.SetDistance(50.0f);
}

void Scene::SetSceneMode(SceneMode mode)
{
    if (m_sceneMode == mode)
        return;

    m_sceneMode = mode;

    // Initialize the appropriate scene
    if (mode == SceneMode::SolarSystem)
    {
        InitializeSolarSystem();
    }
    else
    {
        InitializeDefaultScene();
    }
}

void Scene::Update(float deltaTime)
{
    // Apply simulation speed
    float scaledDeltaTime = deltaTime * m_simulationSpeed;

    // Update orbital motion (for solar system)
    if (m_sceneMode == SceneMode::SolarSystem)
    {
        UpdateOrbitalMotion(scaledDeltaTime);
    }

    // Update physics (for asteroids and other objects with velocity)
    UpdatePhysics(scaledDeltaTime);

    // Detect collisions
    DetectCollisions();
}

void Scene::UpdateOrbitalMotion(float deltaTime)
{
    for (auto& obj : m_objects)
    {
        if (obj->isOrbiting)
        {
            // Update angle
            obj->currentAngle += obj->orbitalSpeed * deltaTime;

            // Wrap angle to [0, 2*PI]
            while (obj->currentAngle > 2.0f * static_cast<float>(M_PI))
                obj->currentAngle -= 2.0f * static_cast<float>(M_PI);
            while (obj->currentAngle < 0)
                obj->currentAngle += 2.0f * static_cast<float>(M_PI);

            // Calculate new position
            obj->position.x = obj->orbitCenter.x + obj->orbitalRadius * cos(obj->currentAngle);
            obj->position.y = obj->orbitCenter.y;
            obj->position.z = obj->orbitCenter.z + obj->orbitalRadius * sin(obj->currentAngle);
        }
    }
}

void Scene::UpdatePhysics(float deltaTime)
{
    for (auto& obj : m_objects)
    {
        if (obj->hasVelocity && !obj->isOrbiting)
        {
            // Update position based on velocity
            obj->position.x += obj->velocity.x * deltaTime;
            obj->position.y += obj->velocity.y * deltaTime;
            obj->position.z += obj->velocity.z * deltaTime;
        }
    }
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

    // Render orbits (for solar system)
    if (m_sceneMode == SceneMode::SolarSystem)
    {
        RenderOrbits();
    }

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

void Scene::RenderOrbits()
{
    const XMFLOAT4 orbitColor(0.3f, 0.3f, 0.3f, 1.0f);
    const int segmentCount = 64;

    for (const auto& obj : m_objects)
    {
        if (obj->isOrbiting && obj->orbitalRadius > 0)
        {
            // Draw orbit as a circle
            for (int i = 0; i < segmentCount; ++i)
            {
                float angle1 = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) / segmentCount;
                float angle2 = static_cast<float>(i + 1) * 2.0f * static_cast<float>(M_PI) / segmentCount;

                XMFLOAT3 p1(
                    obj->orbitCenter.x + obj->orbitalRadius * cos(angle1),
                    obj->orbitCenter.y,
                    obj->orbitCenter.z + obj->orbitalRadius * sin(angle1)
                );

                XMFLOAT3 p2(
                    obj->orbitCenter.x + obj->orbitalRadius * cos(angle2),
                    obj->orbitCenter.y,
                    obj->orbitCenter.z + obj->orbitalRadius * sin(angle2)
                );

                m_renderer->DrawLine(p1, p2, orbitColor);
            }
        }
    }
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

void Scene::AddAsteroid(const std::string& name, const XMFLOAT3& position,
                        const XMFLOAT3& velocity, float radius)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name;
    obj->type = GeometryType::Sphere;
    obj->position = position;
    obj->data.sphere.radius = radius;
    obj->color = XMFLOAT4(0.6f, 0.5f, 0.4f, 1.0f); // Brown-ish asteroid color

    // Set velocity
    obj->hasVelocity = true;
    obj->velocity = velocity;

    // Create FCL geometry if driver is connected
    if (m_driver && m_driver->IsConnected())
    {
        obj->fclHandle = m_driver->CreateSphere(position, radius);
    }

    m_objects.push_back(std::move(obj));
}

void Scene::SetAsteroidVelocity(size_t index, const XMFLOAT3& velocity)
{
    if (index >= m_objects.size())
        return;

    auto& obj = m_objects[index];
    obj->hasVelocity = true;
    obj->velocity = velocity;
}
