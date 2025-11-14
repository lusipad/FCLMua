#include "scene.h"
#include "obj_loader.h"
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

void Scene::InitializeCrossroadScene()
{
    ClearAllObjects();

    // Define road parameters
    const float roadWidth = 4.0f;      // Width of each road lane
    const float roadLength = 40.0f;    // Total length of road
    const float roadThickness = 0.1f;  // Thickness of road
    const XMFLOAT4 roadColor(0.3f, 0.3f, 0.3f, 1.0f);  // Dark gray
    const XMFLOAT4 lineColor(1.0f, 1.0f, 1.0f, 1.0f);  // White

    // Create horizontal road (East-West)
    auto roadEW = std::make_unique<SceneObject>();
    roadEW->name = "Road East-West";
    roadEW->type = GeometryType::Box;
    roadEW->position = XMFLOAT3(0, 0, 0);
    roadEW->data.box.extents = XMFLOAT3(roadLength / 2.0f, roadThickness / 2.0f, roadWidth / 2.0f);
    roadEW->color = roadColor;
    if (m_driver && m_driver->IsConnected())
    {
        roadEW->fclHandle = m_driver->CreateBox(roadEW->position, roadEW->data.box.extents);
    }
    m_objects.push_back(std::move(roadEW));

    // Create vertical road (North-South)
    auto roadNS = std::make_unique<SceneObject>();
    roadNS->name = "Road North-South";
    roadNS->type = GeometryType::Box;
    roadNS->position = XMFLOAT3(0, 0, 0);
    roadNS->data.box.extents = XMFLOAT3(roadWidth / 2.0f, roadThickness / 2.0f, roadLength / 2.0f);
    roadNS->color = roadColor;
    if (m_driver && m_driver->IsConnected())
    {
        roadNS->fclHandle = m_driver->CreateBox(roadNS->position, roadNS->data.box.extents);
    }
    m_objects.push_back(std::move(roadNS));

    // Create crossroad boundary lines (white lines marking the intersection)
    const float lineThickness = 0.05f;
    const float lineWidth = 0.15f;
    const float intersectionSize = roadWidth * 1.5f; // Size of the intersection area

    // North boundary line (marks where vehicles entering from south can start turning)
    auto lineNorth = std::make_unique<SceneObject>();
    lineNorth->name = "Line North";
    lineNorth->type = GeometryType::Box;
    lineNorth->position = XMFLOAT3(0, roadThickness, intersectionSize / 2.0f);
    lineNorth->data.box.extents = XMFLOAT3(roadWidth / 2.0f, lineThickness, lineWidth / 2.0f);
    lineNorth->color = lineColor;
    m_objects.push_back(std::move(lineNorth));

    // South boundary line
    auto lineSouth = std::make_unique<SceneObject>();
    lineSouth->name = "Line South";
    lineSouth->type = GeometryType::Box;
    lineSouth->position = XMFLOAT3(0, roadThickness, -intersectionSize / 2.0f);
    lineSouth->data.box.extents = XMFLOAT3(roadWidth / 2.0f, lineThickness, lineWidth / 2.0f);
    lineSouth->color = lineColor;
    m_objects.push_back(std::move(lineSouth));

    // East boundary line
    auto lineEast = std::make_unique<SceneObject>();
    lineEast->name = "Line East";
    lineEast->type = GeometryType::Box;
    lineEast->position = XMFLOAT3(intersectionSize / 2.0f, roadThickness, 0);
    lineEast->data.box.extents = XMFLOAT3(lineWidth / 2.0f, lineThickness, roadWidth / 2.0f);
    lineEast->color = lineColor;
    m_objects.push_back(std::move(lineEast));

    // West boundary line
    auto lineWest = std::make_unique<SceneObject>();
    lineWest->name = "Line West";
    lineWest->type = GeometryType::Box;
    lineWest->position = XMFLOAT3(-intersectionSize / 2.0f, roadThickness, 0);
    lineWest->data.box.extents = XMFLOAT3(lineWidth / 2.0f, lineThickness, roadWidth / 2.0f);
    lineWest->color = lineColor;
    m_objects.push_back(std::move(lineWest));

    // Adjust camera for top-down view of crossroad
    m_camera.SetTarget(XMFLOAT3(0, 0, 0));
    m_camera.SetDistance(35.0f);
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
    else if (mode == SceneMode::CrossroadSimulation)
    {
        InitializeCrossroadScene();
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

    // Update vehicle movement (for crossroad scene)
    if (m_sceneMode == SceneMode::CrossroadSimulation)
    {
        UpdateVehicleMovement(scaledDeltaTime);
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

void Scene::UpdateVehicleMovement(float deltaTime)
{
    const float intersectionBoundary = 6.0f;  // Distance from center where intersection starts
    const float turnRadius = 3.0f;  // Radius for turning arcs

    for (auto& obj : m_objects)
    {
        if (!obj->isVehicle)
            continue;

        // Calculate movement distance for this frame
        float moveDistance = obj->vehicleSpeed * deltaTime;
        obj->distanceTraveled += moveDistance;

        // Check if vehicle has crossed the intersection line
        bool justCrossed = false;
        if (!obj->hasCrossedIntersection)
        {
            // Check based on direction
            switch (obj->vehicleDirection)
            {
            case VehicleDirection::North:
                if (obj->position.z >= -intersectionBoundary)
                {
                    obj->hasCrossedIntersection = true;
                    justCrossed = true;
                }
                break;
            case VehicleDirection::South:
                if (obj->position.z <= intersectionBoundary)
                {
                    obj->hasCrossedIntersection = true;
                    justCrossed = true;
                }
                break;
            case VehicleDirection::East:
                if (obj->position.x >= -intersectionBoundary)
                {
                    obj->hasCrossedIntersection = true;
                    justCrossed = true;
                }
                break;
            case VehicleDirection::West:
                if (obj->position.x <= intersectionBoundary)
                {
                    obj->hasCrossedIntersection = true;
                    justCrossed = true;
                }
                break;
            }

            // If just crossed and needs to turn, set up the turn
            if (justCrossed && obj->movementIntention != MovementIntention::GoStraight)
            {
                // Calculate turn center and target direction
                // This will be used for smooth turning
                obj->targetPosition = obj->position;  // Will be updated below
            }
        }

        // Update position based on current state
        if (!obj->hasCrossedIntersection || obj->movementIntention == MovementIntention::GoStraight)
        {
            // Move straight along current direction
            switch (obj->vehicleDirection)
            {
            case VehicleDirection::North:
                obj->position.z += moveDistance;
                break;
            case VehicleDirection::South:
                obj->position.z -= moveDistance;
                break;
            case VehicleDirection::East:
                obj->position.x += moveDistance;
                break;
            case VehicleDirection::West:
                obj->position.x -= moveDistance;
                break;
            }
        }
        else
        {
            // Vehicle is turning - use smooth arc
            const float turnSpeed = obj->vehicleSpeed / turnRadius;  // Angular velocity

            // Calculate turn angle for this frame
            float turnAngle = 0;
            if (obj->movementIntention == MovementIntention::TurnLeft)
            {
                turnAngle = turnSpeed * deltaTime;  // Counter-clockwise
            }
            else if (obj->movementIntention == MovementIntention::TurnRight)
            {
                turnAngle = -turnSpeed * deltaTime;  // Clockwise
            }

            // Update rotation
            obj->rotation.y += turnAngle;

            // Move in the direction the vehicle is facing
            float facingAngle = obj->rotation.y;
            obj->position.x += moveDistance * sin(facingAngle);
            obj->position.z += moveDistance * cos(facingAngle);

            // Check if turn is complete (approximately 90 degrees)
            float totalTurnAngle = 0;

            // Calculate expected final angle based on initial direction and turn type
            float targetAngle = 0;
            switch (obj->vehicleDirection)
            {
            case VehicleDirection::North:
                targetAngle = 0;
                if (obj->movementIntention == MovementIntention::TurnLeft)
                    targetAngle = -static_cast<float>(M_PI) / 2.0f;  // Turn to West
                else if (obj->movementIntention == MovementIntention::TurnRight)
                    targetAngle = static_cast<float>(M_PI) / 2.0f;   // Turn to East
                break;

            case VehicleDirection::South:
                targetAngle = static_cast<float>(M_PI);
                if (obj->movementIntention == MovementIntention::TurnLeft)
                    targetAngle = static_cast<float>(M_PI) / 2.0f;   // Turn to East
                else if (obj->movementIntention == MovementIntention::TurnRight)
                    targetAngle = -static_cast<float>(M_PI) / 2.0f;  // Turn to West
                break;

            case VehicleDirection::East:
                targetAngle = static_cast<float>(M_PI) / 2.0f;
                if (obj->movementIntention == MovementIntention::TurnLeft)
                    targetAngle = 0;  // Turn to North
                else if (obj->movementIntention == MovementIntention::TurnRight)
                    targetAngle = static_cast<float>(M_PI);  // Turn to South
                break;

            case VehicleDirection::West:
                targetAngle = -static_cast<float>(M_PI) / 2.0f;
                if (obj->movementIntention == MovementIntention::TurnLeft)
                    targetAngle = static_cast<float>(M_PI);  // Turn to South
                else if (obj->movementIntention == MovementIntention::TurnRight)
                    targetAngle = 0;  // Turn to North
                break;
            }

            // Normalize angles to [-PI, PI]
            auto normalizeAngle = [](float angle) {
                while (angle > static_cast<float>(M_PI)) angle -= 2.0f * static_cast<float>(M_PI);
                while (angle < -static_cast<float>(M_PI)) angle += 2.0f * static_cast<float>(M_PI);
                return angle;
            };

            obj->rotation.y = normalizeAngle(obj->rotation.y);
            targetAngle = normalizeAngle(targetAngle);

            // Check if turn is complete (within tolerance)
            float angleDiff = std::abs(normalizeAngle(obj->rotation.y - targetAngle));
            if (angleDiff < 0.1f)  // Small tolerance
            {
                // Snap to final angle and update direction
                obj->rotation.y = targetAngle;

                // Update the vehicle's direction for future straight movement
                if (obj->movementIntention == MovementIntention::TurnLeft)
                {
                    switch (obj->vehicleDirection)
                    {
                    case VehicleDirection::North: obj->vehicleDirection = VehicleDirection::West; break;
                    case VehicleDirection::South: obj->vehicleDirection = VehicleDirection::East; break;
                    case VehicleDirection::East: obj->vehicleDirection = VehicleDirection::North; break;
                    case VehicleDirection::West: obj->vehicleDirection = VehicleDirection::South; break;
                    }
                }
                else if (obj->movementIntention == MovementIntention::TurnRight)
                {
                    switch (obj->vehicleDirection)
                    {
                    case VehicleDirection::North: obj->vehicleDirection = VehicleDirection::East; break;
                    case VehicleDirection::South: obj->vehicleDirection = VehicleDirection::West; break;
                    case VehicleDirection::East: obj->vehicleDirection = VehicleDirection::South; break;
                    case VehicleDirection::West: obj->vehicleDirection = VehicleDirection::North; break;
                    }
                }

                // Set movement to straight now that turn is complete
                obj->movementIntention = MovementIntention::GoStraight;
                obj->hasCrossedIntersection = false;  // Reset for potential future intersections
            }
        }

        // Update FCL transform if collision detection is active
        if (m_driver && m_driver->IsConnected() && obj->fclHandle.Value != 0)
        {
            FCL_TRANSFORM transform = FclDriver::CreateTransform(
                obj->position, obj->GetRotationMatrix());
            m_driver->UpdateTransform(obj->fclHandle, transform);
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

// Helper function to create vehicle mesh vertices
static void CreateVehicleMesh(VehicleType type,
                             std::vector<XMFLOAT3>& vertices,
                             std::vector<uint32_t>& indices)
{
    vertices.clear();
    indices.clear();

    // We'll create detailed box-based vehicles with multiple components
    // All measurements are relative to a standard sedan size

    switch (type)
    {
    case VehicleType::Sedan:
    {
        // Sedan: compact car with main body + cabin
        float length = 2.5f, width = 1.2f, height = 0.8f;
        float cabinHeight = 0.6f, cabinLength = 1.2f;

        // Main body (lower part)
        std::vector<XMFLOAT3> bodyVerts = {
            // Bottom face
            {-length/2, 0, -width/2}, {length/2, 0, -width/2}, {length/2, 0, width/2}, {-length/2, 0, width/2},
            // Top face
            {-length/2, height, -width/2}, {length/2, height, -width/2}, {length/2, height, width/2}, {-length/2, height, width/2}
        };
        vertices.insert(vertices.end(), bodyVerts.begin(), bodyVerts.end());

        // Cabin (upper part) - centered and shorter
        float cabinOffset = (length - cabinLength) / 4.0f;
        std::vector<XMFLOAT3> cabinVerts = {
            {-cabinLength/2 + cabinOffset, height, -width/2.5f}, {cabinLength/2 + cabinOffset, height, -width/2.5f},
            {cabinLength/2 + cabinOffset, height, width/2.5f}, {-cabinLength/2 + cabinOffset, height, width/2.5f},
            {-cabinLength/2 + cabinOffset, height + cabinHeight, -width/2.5f}, {cabinLength/2 + cabinOffset, height + cabinHeight, -width/2.5f},
            {cabinLength/2 + cabinOffset, height + cabinHeight, width/2.5f}, {-cabinLength/2 + cabinOffset, height + cabinHeight, width/2.5f}
        };
        vertices.insert(vertices.end(), cabinVerts.begin(), cabinVerts.end());
        break;
    }

    case VehicleType::SUV:
    {
        // SUV: larger and taller than sedan
        float length = 3.0f, width = 1.5f, height = 1.2f;
        float cabinHeight = 0.8f, cabinLength = 1.8f;

        std::vector<XMFLOAT3> bodyVerts = {
            {-length/2, 0, -width/2}, {length/2, 0, -width/2}, {length/2, 0, width/2}, {-length/2, 0, width/2},
            {-length/2, height, -width/2}, {length/2, height, -width/2}, {length/2, height, width/2}, {-length/2, height, width/2}
        };
        vertices.insert(vertices.end(), bodyVerts.begin(), bodyVerts.end());

        std::vector<XMFLOAT3> cabinVerts = {
            {-cabinLength/2, height, -width/2.3f}, {cabinLength/2, height, -width/2.3f},
            {cabinLength/2, height, width/2.3f}, {-cabinLength/2, height, width/2.3f},
            {-cabinLength/2, height + cabinHeight, -width/2.3f}, {cabinLength/2, height + cabinHeight, -width/2.3f},
            {cabinLength/2, height + cabinHeight, width/2.3f}, {-cabinLength/2, height + cabinHeight, width/2.3f}
        };
        vertices.insert(vertices.end(), cabinVerts.begin(), cabinVerts.end());
        break;
    }

    case VehicleType::Truck:
    {
        // Truck: long cargo area + small cabin
        float length = 4.0f, width = 1.6f, height = 1.0f;
        float cabinHeight = 1.0f, cabinLength = 1.0f;
        float cargoLength = 2.5f;

        // Cargo area (back)
        std::vector<XMFLOAT3> cargoVerts = {
            {-length/2, 0, -width/2}, {length/2 - cabinLength, 0, -width/2},
            {length/2 - cabinLength, 0, width/2}, {-length/2, 0, width/2},
            {-length/2, height, -width/2}, {length/2 - cabinLength, height, -width/2},
            {length/2 - cabinLength, height, width/2}, {-length/2, height, width/2}
        };
        vertices.insert(vertices.end(), cargoVerts.begin(), cargoVerts.end());

        // Cabin (front)
        std::vector<XMFLOAT3> cabinVerts = {
            {length/2 - cabinLength, 0, -width/2.5f}, {length/2, 0, -width/2.5f},
            {length/2, 0, width/2.5f}, {length/2 - cabinLength, 0, width/2.5f},
            {length/2 - cabinLength, cabinHeight, -width/2.5f}, {length/2, cabinHeight, -width/2.5f},
            {length/2, cabinHeight, width/2.5f}, {length/2 - cabinLength, cabinHeight, width/2.5f}
        };
        vertices.insert(vertices.end(), cabinVerts.begin(), cabinVerts.end());
        break;
    }

    case VehicleType::Bus:
    {
        // Bus: very long and tall
        float length = 5.0f, width = 1.8f, height = 2.0f;

        std::vector<XMFLOAT3> bodyVerts = {
            {-length/2, 0, -width/2}, {length/2, 0, -width/2}, {length/2, 0, width/2}, {-length/2, 0, width/2},
            {-length/2, height, -width/2}, {length/2, height, -width/2}, {length/2, height, width/2}, {-length/2, height, width/2}
        };
        vertices.insert(vertices.end(), bodyVerts.begin(), bodyVerts.end());
        break;
    }

    case VehicleType::SportsCar:
    {
        // Sports car: low and sleek
        float length = 2.8f, width = 1.4f, height = 0.6f;
        float cabinHeight = 0.4f, cabinLength = 1.0f;

        std::vector<XMFLOAT3> bodyVerts = {
            {-length/2, 0, -width/2}, {length/2, 0, -width/2}, {length/2, 0, width/2}, {-length/2, 0, width/2},
            {-length/2, height, -width/2}, {length/2, height, -width/2}, {length/2, height, width/2}, {-length/2, height, width/2}
        };
        vertices.insert(vertices.end(), bodyVerts.begin(), bodyVerts.end());

        std::vector<XMFLOAT3> cabinVerts = {
            {-cabinLength/2, height, -width/3.0f}, {cabinLength/2, height, -width/3.0f},
            {cabinLength/2, height, width/3.0f}, {-cabinLength/2, height, width/3.0f},
            {-cabinLength/2.5f, height + cabinHeight, -width/3.5f}, {cabinLength/2.5f, height + cabinHeight, -width/3.5f},
            {cabinLength/2.5f, height + cabinHeight, width/3.5f}, {-cabinLength/2.5f, height + cabinHeight, width/3.5f}
        };
        vertices.insert(vertices.end(), cabinVerts.begin(), cabinVerts.end());
        break;
    }
    }

    // Generate indices for all boxes (each component is a box with 6 faces)
    size_t numBoxes = vertices.size() / 8;  // Each box has 8 vertices
    for (size_t box = 0; box < numBoxes; ++box)
    {
        uint32_t base = static_cast<uint32_t>(box * 8);

        // Define 6 faces (2 triangles each)
        uint32_t boxIndices[] = {
            // Bottom
            base+0, base+1, base+2,  base+0, base+2, base+3,
            // Top
            base+4, base+6, base+5,  base+4, base+7, base+6,
            // Front
            base+0, base+4, base+5,  base+0, base+5, base+1,
            // Back
            base+2, base+6, base+7,  base+2, base+7, base+3,
            // Left
            base+0, base+3, base+7,  base+0, base+7, base+4,
            // Right
            base+1, base+5, base+6,  base+1, base+6, base+2
        };

        indices.insert(indices.end(), boxIndices, boxIndices + 36);
    }
}

void Scene::AddVehicle(const std::string& name, VehicleType type,
                       VehicleDirection direction, MovementIntention intention,
                       float speed)
{
    // Create vehicle mesh
    std::vector<XMFLOAT3> vertices;
    std::vector<uint32_t> indices;
    CreateVehicleMesh(type, vertices, indices);

    // Determine starting position based on direction
    XMFLOAT3 startPos;
    const float laneOffset = 1.0f;  // Offset from center of road
    const float startDistance = 18.0f; // Distance from center

    switch (direction)
    {
    case VehicleDirection::North:
        startPos = XMFLOAT3(-laneOffset, 0.5f, -startDistance);
        break;
    case VehicleDirection::South:
        startPos = XMFLOAT3(laneOffset, 0.5f, startDistance);
        break;
    case VehicleDirection::East:
        startPos = XMFLOAT3(-startDistance, 0.5f, laneOffset);
        break;
    case VehicleDirection::West:
        startPos = XMFLOAT3(startDistance, 0.5f, -laneOffset);
        break;
    }

    // Create the vehicle object
    auto obj = std::make_unique<SceneObject>();
    obj->name = name;
    obj->type = GeometryType::Mesh;
    obj->position = startPos;

    // Set vehicle-specific data
    obj->isVehicle = true;
    obj->vehicleType = type;
    obj->vehicleDirection = direction;
    obj->movementIntention = intention;
    obj->vehicleSpeed = speed;
    obj->hasCrossedIntersection = false;
    obj->distanceTraveled = 0;

    // Set rotation based on direction
    switch (direction)
    {
    case VehicleDirection::North:
        obj->rotation = XMFLOAT3(0, 0, 0);  // Facing +Z
        break;
    case VehicleDirection::South:
        obj->rotation = XMFLOAT3(0, static_cast<float>(M_PI), 0);  // Facing -Z
        break;
    case VehicleDirection::East:
        obj->rotation = XMFLOAT3(0, static_cast<float>(M_PI) / 2.0f, 0);  // Facing +X
        break;
    case VehicleDirection::West:
        obj->rotation = XMFLOAT3(0, -static_cast<float>(M_PI) / 2.0f, 0);  // Facing -X
        break;
    }

    // Assign color based on vehicle type
    switch (type)
    {
    case VehicleType::Sedan:
        obj->color = XMFLOAT4(0.8f, 0.2f, 0.2f, 1.0f);  // Red
        break;
    case VehicleType::SUV:
        obj->color = XMFLOAT4(0.2f, 0.4f, 0.8f, 1.0f);  // Blue
        break;
    case VehicleType::Truck:
        obj->color = XMFLOAT4(0.6f, 0.6f, 0.2f, 1.0f);  // Yellow
        break;
    case VehicleType::Bus:
        obj->color = XMFLOAT4(0.9f, 0.5f, 0.1f, 1.0f);  // Orange
        break;
    case VehicleType::SportsCar:
        obj->color = XMFLOAT4(0.1f, 0.8f, 0.3f, 1.0f);  // Green
        break;
    }

    // Create mesh in renderer
    if (m_renderer)
    {
        auto mesh = m_renderer->CreateMesh(vertices, indices);
        obj->data.customMesh.mesh = mesh;

        // Create FCL collision geometry (use bounding box for simplicity)
        if (m_driver && m_driver->IsConnected())
        {
            // Calculate bounding box
            XMFLOAT3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
            XMFLOAT3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            for (const auto& v : vertices)
            {
                minBounds.x = std::min(minBounds.x, v.x);
                minBounds.y = std::min(minBounds.y, v.y);
                minBounds.z = std::min(minBounds.z, v.z);
                maxBounds.x = std::max(maxBounds.x, v.x);
                maxBounds.y = std::max(maxBounds.y, v.y);
                maxBounds.z = std::max(maxBounds.z, v.z);
            }
            XMFLOAT3 extents(
                (maxBounds.x - minBounds.x) / 2.0f,
                (maxBounds.y - minBounds.y) / 2.0f,
                (maxBounds.z - minBounds.z) / 2.0f
            );
            obj->fclHandle = m_driver->CreateBox(startPos, extents);
        }
    }

    m_objects.push_back(std::move(obj));
}

void Scene::AddVehicleFromOBJ(const std::string& name, const std::string& objFilePath,
                              VehicleDirection direction, MovementIntention intention,
                              float speed, float scale)
{
    // Load OBJ file
    ObjLoader::MeshData meshData;
    if (!ObjLoader::LoadFromFile(objFilePath, meshData, scale))
    {
        // Failed to load OBJ file
        return;
    }

    // Determine starting position based on direction
    XMFLOAT3 startPos;
    const float laneOffset = 1.0f;  // Offset from center of road
    const float startDistance = 18.0f; // Distance from center

    switch (direction)
    {
    case VehicleDirection::North:
        startPos = XMFLOAT3(-laneOffset, 0.5f, -startDistance);
        break;
    case VehicleDirection::South:
        startPos = XMFLOAT3(laneOffset, 0.5f, startDistance);
        break;
    case VehicleDirection::East:
        startPos = XMFLOAT3(-startDistance, 0.5f, laneOffset);
        break;
    case VehicleDirection::West:
        startPos = XMFLOAT3(startDistance, 0.5f, -laneOffset);
        break;
    }

    // Create the vehicle object
    auto obj = std::make_unique<SceneObject>();
    obj->name = name;
    obj->type = GeometryType::Mesh;
    obj->position = startPos;

    // Set vehicle-specific data
    obj->isVehicle = true;
    obj->vehicleType = VehicleType::Sedan;  // Default, will be custom
    obj->vehicleDirection = direction;
    obj->movementIntention = intention;
    obj->vehicleSpeed = speed;
    obj->hasCrossedIntersection = false;
    obj->distanceTraveled = 0;

    // Set rotation based on direction
    switch (direction)
    {
    case VehicleDirection::North:
        obj->rotation = XMFLOAT3(0, 0, 0);  // Facing +Z
        break;
    case VehicleDirection::South:
        obj->rotation = XMFLOAT3(0, static_cast<float>(M_PI), 0);  // Facing -Z
        break;
    case VehicleDirection::East:
        obj->rotation = XMFLOAT3(0, static_cast<float>(M_PI) / 2.0f, 0);  // Facing +X
        break;
    case VehicleDirection::West:
        obj->rotation = XMFLOAT3(0, -static_cast<float>(M_PI) / 2.0f, 0);  // Facing -X
        break;
    }

    // Use a distinct color for custom OBJ models
    obj->color = XMFLOAT4(0.7f, 0.3f, 0.9f, 1.0f);  // Purple for custom models

    // Create mesh in renderer
    if (m_renderer)
    {
        auto mesh = m_renderer->CreateMesh(meshData.vertices, meshData.indices);
        obj->data.customMesh.mesh = mesh;

        // Create FCL collision geometry using bounding box
        if (m_driver && m_driver->IsConnected())
        {
            XMFLOAT3 extents(
                (meshData.maxBounds.x - meshData.minBounds.x) / 2.0f,
                (meshData.maxBounds.y - meshData.minBounds.y) / 2.0f,
                (meshData.maxBounds.z - meshData.minBounds.z) / 2.0f
            );
            obj->fclHandle = m_driver->CreateBox(startPos, extents);
        }
    }

    m_objects.push_back(std::move(obj));
}
