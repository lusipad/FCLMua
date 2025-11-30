#include "scene.h"
#include "obj_loader.h"
#include <algorithm>
#include <fstream>
#include <cmath>

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
    , m_performanceMode(PerformanceMode::High)
    , m_collisionFrameSkipCounter(0)
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
    sun->data.sphere.radius = 2.0f;  // Reduced from 3.0f to avoid overlap with Mercury (orbit 5.0f)
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
    const float roadThickness = 0.02f; // Very thin, just above grid
    const XMFLOAT4 lineColor(1.0f, 1.0f, 1.0f, 1.0f);  // White
    const XMFLOAT4 yellowColor(1.0f, 0.8f, 0.0f, 1.0f); // Yellow
    const XMFLOAT4 poleColor(0.4f, 0.4f, 0.4f, 1.0f);   // Grey metal
    const XMFLOAT4 lightBoxColor(0.2f, 0.2f, 0.2f, 1.0f); // Black housing

    // Helper to add a zebra crossing
    auto AddZebraCrossing = [&](const XMFLOAT3& center, bool horizontal) {
        int stripes = 6;
        float stripeW = 0.6f;
        float stripeL = 3.0f; // Length of crossing
        float gap = 0.4f;
        float totalW = stripes * stripeW + (stripes - 1) * gap;
        float startOffset = -totalW / 2.0f + stripeW / 2.0f;

        for (int i = 0; i < stripes; ++i) {
            float offset = startOffset + i * (stripeW + gap);
            XMFLOAT3 pos = center;
            XMFLOAT3 size;
            
            if (horizontal) { // Stripes run Z, arranged along X
                pos.x += offset;
                size = XMFLOAT3(stripeW/2, roadThickness, stripeL/2);
            } else { // Stripes run X, arranged along Z
                pos.z += offset;
                size = XMFLOAT3(stripeL/2, roadThickness, stripeW/2);
            }
            
            auto stripe = std::make_unique<SceneObject>();
            stripe->name = "Zebra Stripe";
            stripe->type = GeometryType::Box;
            stripe->position = pos;
            stripe->data.box.extents = size;
            stripe->color = lineColor;
            m_objects.push_back(std::move(stripe));
        }
    };

    // Helper to add dashed lane lines
    auto AddDashedLine = [&](const XMFLOAT3& start, const XMFLOAT3& end) {
        float len = sqrt(pow(end.x - start.x, 2) + pow(end.z - start.z, 2));
        float dashLen = 1.5f;
        float gapLen = 1.5f;
        int count = (int)(len / (dashLen + gapLen));
        
        float dx = (end.x - start.x) / len;
        float dz = (end.z - start.z) / len;
        
        for (int i = 0; i < count; ++i) {
            float dist = i * (dashLen + gapLen) + gapLen; // Start with gap
            XMFLOAT3 center(start.x + dx * (dist + dashLen/2), roadThickness, start.z + dz * (dist + dashLen/2));
            
            auto dash = std::make_unique<SceneObject>();
            dash->name = "Lane Dash";
            dash->type = GeometryType::Box;
            dash->position = center;
            if (abs(dx) > abs(dz)) // Horizontal line
                dash->data.box.extents = XMFLOAT3(dashLen/2, roadThickness, 0.08f);
            else // Vertical line
                dash->data.box.extents = XMFLOAT3(0.08f, roadThickness, dashLen/2);
            dash->color = lineColor;
            m_objects.push_back(std::move(dash));
        }
    };

    // Helper to add double yellow center lines
    auto AddDoubleYellow = [&](const XMFLOAT3& center, float length, bool horizontal) {
        float lineW = 0.1f;
        float sep = 0.15f;
        XMFLOAT3 size = horizontal ? XMFLOAT3(length/2, roadThickness, lineW) : XMFLOAT3(lineW, roadThickness, length/2);
        
        auto l1 = std::make_unique<SceneObject>();
        l1->type = GeometryType::Box; l1->data.box.extents = size; l1->color = yellowColor;
        l1->position = center; 
        if (horizontal) l1->position.z -= sep; else l1->position.x -= sep;
        m_objects.push_back(std::move(l1));

        auto l2 = std::make_unique<SceneObject>();
        l2->type = GeometryType::Box; l2->data.box.extents = size; l2->color = yellowColor;
        l2->position = center;
        if (horizontal) l2->position.z += sep; else l2->position.x += sep;
        m_objects.push_back(std::move(l2));
    };

    // Helper to add Traffic Light
    auto AddTrafficLight = [&](const XMFLOAT3& pos, float rotationY) {
        float poleH = 6.0f;
        float armLen = 5.0f;
        
        // Pole
        auto pole = std::make_unique<SceneObject>();
        pole->type = GeometryType::Box; // Cylinder would be better but Box is fine
        pole->position = XMFLOAT3(pos.x, poleH/2, pos.z);
        pole->data.box.extents = XMFLOAT3(0.15f, poleH/2, 0.15f);
        pole->color = poleColor;
        m_objects.push_back(std::move(pole));
        
        // Arm
        auto arm = std::make_unique<SceneObject>();
        arm->type = GeometryType::Box;
        // Arm extends towards center (0,0,0). 
        // Assuming pos is at corner, rotation tells us direction.
        // Rotation 0 = Arm along +X? 
        // Let's simplify: Arm extends in the direction of rotationY
        float armX = pos.x + (armLen/2) * sin(rotationY);
        float armZ = pos.z + (armLen/2) * cos(rotationY);
        
        arm->position = XMFLOAT3(armX, poleH - 0.5f, armZ);
        arm->rotation = XMFLOAT3(0, rotationY, 0);
        arm->data.box.extents = XMFLOAT3(0.1f, 0.1f, armLen/2); // Length along Z local
        arm->color = poleColor;
        m_objects.push_back(std::move(arm));
        
        // Light Box
        float boxW = 0.4f; float boxH = 1.2f; float boxD = 0.4f;
        // Position at end of arm
        float endX = pos.x + (armLen - 1.0f) * sin(rotationY);
        float endZ = pos.z + (armLen - 1.0f) * cos(rotationY);
        
        auto box = std::make_unique<SceneObject>();
        box->type = GeometryType::Box;
        box->position = XMFLOAT3(endX, poleH - 1.0f, endZ);
        box->rotation = XMFLOAT3(0, rotationY, 0);
        box->data.box.extents = XMFLOAT3(boxW, boxH, boxD);
        box->color = lightBoxColor;
        m_objects.push_back(std::move(box));
        
        // Lights (Red, Yellow, Green)
        auto AddLamp = [&](float yOffset, const XMFLOAT4& c) {
            auto lamp = std::make_unique<SceneObject>();
            lamp->type = GeometryType::Sphere;
            lamp->data.sphere.radius = 0.25f;
            // Position slightly forward in direction of rotation
            // Local Z is direction of arm. Light faces perpendicular?
            // If arm is along Z, traffic comes from -Z or +Z.
            // Lights should face -Z (local).
            float localZ = -boxD - 0.1f;
            float worldX = endX + localZ * sin(rotationY);
            float worldZ = endZ + localZ * cos(rotationY);
            
            lamp->position = XMFLOAT3(worldX, poleH - 1.0f + yOffset, worldZ);
            lamp->color = c;
            m_objects.push_back(std::move(lamp));
        };
        
        AddLamp(0.6f, XMFLOAT4(1,0,0,1)); // Red
        AddLamp(0.0f, XMFLOAT4(1,1,0,1)); // Yellow
        AddLamp(-0.6f, XMFLOAT4(0,1,0,1)); // Green
    };

    // --- Build The Scene ---

    const float intersectionSize = roadWidth * 1.5f; 
    const float stopLineDist = intersectionSize / 2.0f + 1.0f;

    // 1. Stop Lines (Solid White)
    auto AddStopLine = [&](float x, float z, bool horizontal) {
        auto line = std::make_unique<SceneObject>();
        line->type = GeometryType::Box;
        line->position = XMFLOAT3(x, roadThickness, z);
        if (horizontal) line->data.box.extents = XMFLOAT3(roadWidth/2, roadThickness, 0.3f);
        else            line->data.box.extents = XMFLOAT3(0.3f, roadThickness, roadWidth/2);
        line->color = lineColor;
        m_objects.push_back(std::move(line));
    };
    
    AddStopLine(0, stopLineDist, true);   // North
    AddStopLine(0, -stopLineDist, true);  // South
    AddStopLine(stopLineDist, 0, false);  // East
    AddStopLine(-stopLineDist, 0, false); // West

    // 2. Zebra Crossings (Behind Stop Lines)
    AddZebraCrossing(XMFLOAT3(0, 0, stopLineDist + 2.5f), true);
    AddZebraCrossing(XMFLOAT3(0, 0, -stopLineDist - 2.5f), true);
    AddZebraCrossing(XMFLOAT3(stopLineDist + 2.5f, 0, 0), false);
    AddZebraCrossing(XMFLOAT3(-stopLineDist - 2.5f, 0, 0), false);

    // 3. Double Yellow Lines (From Zebra outwards)
    float startDist = stopLineDist + 5.0f;
    float lineLen = (roadLength/2 - startDist);
    float centerDist = startDist + lineLen/2;
    
    AddDoubleYellow(XMFLOAT3(0, 0, centerDist), lineLen, false); // North
    AddDoubleYellow(XMFLOAT3(0, 0, -centerDist), lineLen, false); // South
    AddDoubleYellow(XMFLOAT3(centerDist, 0, 0), lineLen, true); // East
    AddDoubleYellow(XMFLOAT3(-centerDist, 0, 0), lineLen, true); // West

    // 4. Lane Markings (Dashed)
    // Assuming 2 lanes per direction (roadWidth = 4.0 -> 2.0 per side)
    // Wait, roadWidth 4.0 is total width? Or per side?
    // In previous code: extents = roadWidth/2. So total width = roadWidth = 4.0.
    // That's barely enough for 1 lane each way (lane ~3.5m).
    // So no dashed lines needed for single lane roads. 
    // Let's widen the road logically for dashed lines? 
    // Or just assume it's a single lane road with double yellow center.
    
    // Let's add dashed lines on the *edges* (Shoulder lines)?
    // No, let's keep it clean. Double yellow is enough for 2-lane road.

    // 5. Traffic Lights (Removed as per user request)
    /*
    float poleDist = intersectionSize/2.0f + 2.0f;
    // NE Corner -> Faces West/South traffic
    AddTrafficLight(XMFLOAT3(poleDist, 0, poleDist), 3.14159f + 3.14159f/2.0f); // -90 deg?
    // SE Corner
    AddTrafficLight(XMFLOAT3(poleDist, 0, -poleDist), 3.14159f);
    // SW Corner
    AddTrafficLight(XMFLOAT3(-poleDist, 0, -poleDist), 3.14159f/2.0f);
    // NW Corner
    AddTrafficLight(XMFLOAT3(-poleDist, 0, poleDist), 0.0f);
    */

    // Add default sample vehicles for demonstration
    // These vehicles showcase different types, directions, and movement intentions

    // 1. Red Sedan going straight from South to North (speed 3.0)
    AddVehicle("Demo Sedan", VehicleType::Sedan,
               VehicleDirection::North, MovementIntention::GoStraight, 3.0f);

    // 2. Blue SUV turning left from East to North (speed 2.5)
    AddVehicle("Demo SUV", VehicleType::SUV,
               VehicleDirection::East, MovementIntention::TurnLeft, 2.5f);

    // 3. Yellow Truck going straight from West to East (speed 2.0)
    AddVehicle("Demo Truck", VehicleType::Truck,
               VehicleDirection::West, MovementIntention::GoStraight, 2.0f);

    // 4. Green Sports Car turning right from North to East (speed 4.0)
    AddVehicle("Demo Sports Car", VehicleType::SportsCar,
               VehicleDirection::South, MovementIntention::TurnRight, 4.0f);

    // 5. Orange Bus going straight from North to South (speed 1.5)
    AddVehicle("Demo Bus", VehicleType::Bus,
               VehicleDirection::North, MovementIntention::GoStraight, 1.5f);

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

    // Detect collisions with frame skip based on performance mode
    int skipInterval = 0;
    switch (m_performanceMode)
    {
    case PerformanceMode::High:
        skipInterval = 0;  // Every frame
        break;
    case PerformanceMode::Medium:
        skipInterval = 1;  // Every 2nd frame
        break;
    case PerformanceMode::Low:
        skipInterval = 2;  // Every 3rd frame
        break;
    }

    if (skipInterval == 0 || m_collisionFrameSkipCounter >= skipInterval)
    {
        DetectCollisions();
        m_collisionFrameSkipCounter = 0;
    }
    else
    {
        m_collisionFrameSkipCounter++;
    }
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
                obj->position, obj->GetRotationMatrix(), obj->scale);
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
    m_renderer->SetViewProjection(view, proj, m_camera.GetPosition());

    // Render grid (Skip for Solar System)
    if (m_sceneMode != SceneMode::SolarSystem)
    {
        RenderGrid();
    }
    m_renderer->FlushLines(); // Draw grid before objects

    // Render orbits (for solar system)
    if (m_sceneMode == SceneMode::SolarSystem)
    {
        RenderOrbits();
        m_renderer->FlushLines(); // Draw orbits before objects
    }

    // Render objects
    RenderObjects();

    // Render gizmo for selected object
    if (GetSelectedObject())
    {
        RenderGizmo();
    }
    
    // Render contact points
    RenderContacts();
    
    // Flush any remaining lines (gizmos, etc)
    m_renderer->FlushLines();
}

void Scene::RenderContacts()
{
    if (m_contacts.empty())
        return;

    for (const auto& cp : m_contacts)
    {
        // Draw a small yellow sphere at contact point
        // Note: DrawSphere takes position, radius, color
        m_renderer->DrawSphere(cp.position, 0.15f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));
        
        // Draw normal line (yellow)
        // We need to implement DrawLine in Renderer, or just use a thin box/cylinder for now if DrawLine is missing
        // Since Renderer::DrawLine is a TODO, let's use a small box stretched along the normal
        // For simplicity in this demo, just the sphere is a good start.
        // If we want the normal, we can try to draw a second smaller sphere along the normal
        XMFLOAT3 normalTip;
        normalTip.x = cp.position.x + cp.normal.x * 0.5f;
        normalTip.y = cp.position.y + cp.normal.y * 0.5f;
        normalTip.z = cp.position.z + cp.normal.z * 0.5f;
        m_renderer->DrawSphere(normalTip, 0.05f, XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f)); // Orange tip
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

        // Apply rotation based on mouse delta (skip only if delta is zero)
        if (mouseDX != 0 || mouseDY != 0)
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

        // Apply panning based on mouse delta (skip only if delta is zero)
        if (mouseDX != 0 || mouseDY != 0)
        {
            float sensitivity = 0.01f * m_camera.GetDistance();
            m_camera.Pan(-mouseDX * sensitivity, mouseDY * sensitivity);
        }
    }
    else
    {
        m_isPanning = false;
    }

    // Camera zoom (Mouse wheel) - improved with better scaling
    int mouseWheel = window->GetMouseWheel();
    if (mouseWheel != 0)
    {
        float zoomAmount = mouseWheel / 120.0f;
        // Use a smoother zoom factor
        m_camera.Zoom(-zoomAmount * m_camera.GetDistance() * 0.15f);
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
    static bool ctrlSPressed = false;
    static bool ctrlOPressed = false;

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

    // Save scene (Ctrl+S)
    if (ctrlDown && window->IsKeyDown('S'))
    {
        if (!ctrlSPressed)
        {
            if (SaveScene("scene_save.txt"))
            {
                window->SetStatusText(L"Scene saved to scene_save.txt");
            }
            else
            {
                window->SetStatusText(L"Failed to save scene");
            }
            ctrlSPressed = true;
        }
    }
    else
    {
        ctrlSPressed = false;
    }

    // Load scene (Ctrl+O)
    if (ctrlDown && window->IsKeyDown('O'))
    {
        if (!ctrlOPressed)
        {
            if (LoadScene("scene_save.txt"))
            {
                window->SetStatusText(L"Scene loaded from scene_save.txt");
            }
            else
            {
                window->SetStatusText(L"Failed to load scene (file not found or invalid format)");
            }
            ctrlOPressed = true;
        }
    }
    else
    {
        ctrlOPressed = false;
    }

    // Reset mouse input after processing to ensure smooth input next frame
    window->ResetMouseInput();
}

void Scene::RenderObjects()
{
    // 1. Draw Shadows (Blob)
    // Skip shadows in Solar System mode (space) and Crossroad mode (clean grid)
    if (m_sceneMode != SceneMode::SolarSystem && m_sceneMode != SceneMode::CrossroadSimulation)
    {
        m_renderer->SetAlphaBlending(true);
        for (const auto& obj : m_objects)
        {
            // Skip shadow for orbiting objects (planets)
            if (obj->isOrbiting) continue;

            // Simple shadow position
            XMFLOAT3 shadowPos = obj->position;
            shadowPos.y = 0.02f; // Slightly above grid
            
            // Determine size
            float size = 1.0f;
            if (obj->type == GeometryType::Sphere) size = obj->data.sphere.radius;
            else if (obj->type == GeometryType::Box) size = (std::max)(obj->data.box.extents.x, obj->data.box.extents.z);
            else size = 1.5f; // Approx for mesh
            
            // Flattened sphere for shadow
            XMMATRIX shadowWorld = XMMatrixScaling(size * 1.4f, 0.05f, size * 1.4f) * 
                                   XMMatrixTranslation(shadowPos.x, shadowPos.y, shadowPos.z);
            
            // Draw dark semi-transparent shadow
            m_renderer->DrawMesh(*m_renderer->GetSphereMesh(), shadowWorld, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.4f));
        }
        m_renderer->SetAlphaBlending(false);
    }

    // 2. Draw Objects
    for (const auto& obj : m_objects)
    {
        // Determine color (highlight if selected or colliding)
        XMFLOAT4 color = obj->color;
        // Collision has higher priority than selection for visualization.
        if (obj->isColliding)
        {
            color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f); // Red for colliding
        }
        else if (obj->isSelected)
        {
            color = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow for selected
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
    m_contacts.clear();

    // Update statistics for this frame
    m_collisionStats.FrameCount++;
    m_collisionStats.LastFramePairs = 0;
    m_collisionStats.LastFrameHits = 0;

    const bool crossroadMode = (m_sceneMode == SceneMode::CrossroadSimulation);
    const bool solarSystemMode = (m_sceneMode == SceneMode::SolarSystem);

    // Check all pairs
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        for (size_t j = i + 1; j < m_objects.size(); ++j)
        {
            auto& obj1 = m_objects[i];
            auto& obj2 = m_objects[j];

            if (obj1->fclHandle.Value == 0 || obj2->fclHandle.Value == 0)
                continue;

            // Reset contacts at start of frame (done in DetectCollisions start)
            // But we are in a loop.
            // Actually, m_contacts should be cleared at the beginning of DetectCollisions.


            // In crossroad scene, only check vehicle-vehicle pairs to reduce IO and focus on cars.
            if (crossroadMode)
            {
                if (!(obj1->isVehicle && obj2->isVehicle))
                {
                    continue;
                }
            }

            // In solar system scene, skip collision detection for orbiting objects
            // Only check collisions between asteroids (hasVelocity) and other objects
            if (solarSystemMode)
            {
                // Skip if both objects are orbiting (planets don't collide with each other or the sun)
                if (obj1->isOrbiting && obj2->isOrbiting)
                {
                    continue;
                }

                // Skip if one is orbiting and the other has no velocity (sun vs planets)
                if ((obj1->isOrbiting && !obj2->hasVelocity) ||
                    (obj2->isOrbiting && !obj1->hasVelocity))
                {
                    continue;
                }

                // Only check: asteroids (hasVelocity) vs anything
            }

            // Broadphase: AABB Check
            // Calculate AABBs roughly based on position and scale/radius
            float r1 = 1.0f;
            if (obj1->type == GeometryType::Sphere) r1 = obj1->data.sphere.radius;
            else if (obj1->type == GeometryType::Box) r1 = max(max(obj1->data.box.extents.x, obj1->data.box.extents.y), obj1->data.box.extents.z);
            else r1 = 3.0f; // Approximate for mesh

            float r2 = 1.0f;
            if (obj2->type == GeometryType::Sphere) r2 = obj2->data.sphere.radius;
            else if (obj2->type == GeometryType::Box) r2 = max(max(obj2->data.box.extents.x, obj2->data.box.extents.y), obj2->data.box.extents.z);
            else r2 = 3.0f;

            // Distance check (Squared to avoid sqrt)
            float dx = obj1->position.x - obj2->position.x;
            float dy = obj1->position.y - obj2->position.y;
            float dz = obj1->position.z - obj2->position.z;
            float distSq = dx*dx + dy*dy + dz*dz;
            float radSum = r1 + r2;

            // If distance is greater than sum of radii, they cannot collide
            if (distSq > radSum * radSum)
            {
                continue; // Skip expensive kernel call
            }

            // Create transforms (include scale to match rendering)
            FCL_TRANSFORM transform1 = FclDriver::CreateTransform(obj1->position, obj1->GetRotationMatrix(), obj1->scale);
            FCL_TRANSFORM transform2 = FclDriver::CreateTransform(obj2->position, obj2->GetRotationMatrix(), obj2->scale);

            // Query collision
            bool isColliding = false;
            FCL_CONTACT_INFO contactInfo;

            if (m_driver->QueryCollision(obj1->fclHandle, transform1, obj2->fclHandle, transform2,
                                         isColliding, contactInfo))
            {
                m_collisionStats.LastFramePairs++;
                m_collisionStats.TotalPairs++;

                if (isColliding)
                {
                    obj1->isColliding = true;
                    obj2->isColliding = true;
                    m_collisionStats.LastFrameHits++;
                    m_collisionStats.TotalHits++;

                    // Store contact point
                    ContactPoint cp;
                    // Use PointOnObject1 as the contact position for visualization
                    cp.position = FclDriver::FromFclVector(contactInfo.PointOnObject1);
                    cp.normal = FclDriver::FromFclVector(contactInfo.Normal);
                    cp.depth = contactInfo.PenetrationDepth;
                    m_contacts.push_back(cp);
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

// Helper function to add a box to the mesh
static void AddBoxToMesh(std::vector<Vertex>& vertices,
                         std::vector<uint32_t>& indices,
                         const XMFLOAT3& center,
                         const XMFLOAT3& extents,
                         const XMFLOAT4& color)
{
    const uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

    XMFLOAT3 boxVerts[8] = {
        {center.x - extents.x, center.y - extents.y, center.z - extents.z},
        {center.x + extents.x, center.y - extents.y, center.z - extents.z},
        {center.x + extents.x, center.y - extents.y, center.z + extents.z},
        {center.x - extents.x, center.y - extents.y, center.z + extents.z},
        {center.x - extents.x, center.y + extents.y, center.z - extents.z},
        {center.x + extents.x, center.y + extents.y, center.z - extents.z},
        {center.x + extents.x, center.y + extents.y, center.z + extents.z},
        {center.x - extents.x, center.y + extents.y, center.z + extents.z}
    };

    XMFLOAT3 faceNormals[6] = {
        {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
    };
    
    int faceIndices[6][4] = {
        {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 3, 7, 4}, {1, 2, 6, 5}
    };

    for (int f = 0; f < 6; f++)
    {
        for (int i = 0; i < 4; i++)
        {
            Vertex v;
            v.Position = boxVerts[faceIndices[f][i]];
            v.Normal = faceNormals[f];
            v.Color = color;
            vertices.push_back(v);
        }
        uint32_t base = baseIdx + f * 4;
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    }
}

// Helper to add a cylinder (used for wheels, wheel wells)
static void AddCylinderToMesh(std::vector<Vertex>& vertices,
                              std::vector<uint32_t>& indices,
                              const XMFLOAT3& center,
                              float radius,
                              float width,
                              bool isWheel, // if true, rotate to lie on X axis
                              const XMFLOAT4& color)
{
    const int segments = 24;
    const float halfWidth = width / 2.0f;
    const uint32_t centerLeftIdx = static_cast<uint32_t>(vertices.size());
    const uint32_t centerRightIdx = centerLeftIdx + 1;
    
    // Center points for caps
    Vertex cL, cR;
    cL.Color = color; cR.Color = color;
    
    if (isWheel) {
        cL.Position = {center.x - halfWidth, center.y, center.z}; cL.Normal = {-1, 0, 0};
        cR.Position = {center.x + halfWidth, center.y, center.z}; cR.Normal = {1, 0, 0};
    } else {
        cL.Position = {center.x, center.y, center.z - halfWidth}; cL.Normal = {0, 0, -1};
        cR.Position = {center.x, center.y, center.z + halfWidth}; cR.Normal = {0, 0, 1};
    }
    
    vertices.push_back(cL);
    vertices.push_back(cR);
    
    const uint32_t ringStartIdx = static_cast<uint32_t>(vertices.size());
    
    for (int i = 0; i <= segments; i++) // <= to duplicate last vertex for UV/Normal seam if needed, keeping simple here
    {
        float theta = 2.0f * XM_PI * (float)i / (float)segments;
        float c = cos(theta);
        float s = sin(theta);
        
        XMFLOAT3 p1, p2, n;
        
        if (isWheel) {
            // Circle in YZ plane
            p1 = {center.x - halfWidth, center.y + radius * c, center.z + radius * s};
            p2 = {center.x + halfWidth, center.y + radius * c, center.z + radius * s};
            n = {0, c, s};
        } else {
            // Circle in XY plane (not typical for wheels, but generic cylinder)
            p1 = {center.x + radius * c, center.y + radius * s, center.z - halfWidth};
            p2 = {center.x + radius * c, center.y + radius * s, center.z + halfWidth};
            n = {c, s, 0};
        }
        
        Vertex v1; v1.Position = p1; v1.Normal = isWheel ? XMFLOAT3(-1,0,0) : XMFLOAT3(0,0,-1); v1.Color = color;
        Vertex v2; v2.Position = p1; v2.Normal = n; v2.Color = color; // Rim left
        Vertex v3; v3.Position = p2; v3.Normal = n; v3.Color = color; // Rim right
        Vertex v4; v4.Position = p2; v4.Normal = isWheel ? XMFLOAT3(1,0,0) : XMFLOAT3(0,0,1); v4.Color = color;
        
        vertices.push_back(v1); vertices.push_back(v2); vertices.push_back(v3); vertices.push_back(v4);
    }
    
    // Indices
    for (int i = 0; i < segments; i++)
    {
        uint32_t b = ringStartIdx + i * 4;
        
        // Left Cap (Triangle Fan-ish)
        indices.push_back(centerLeftIdx);
        indices.push_back(b + 4); // Next v1
        indices.push_back(b + 0); // Curr v1
        
        // Rim (Quad)
        indices.push_back(b + 1); // Curr v2
        indices.push_back(b + 5); // Next v2
        indices.push_back(b + 2); // Curr v3
        
        indices.push_back(b + 5); // Next v2
        indices.push_back(b + 6); // Next v3
        indices.push_back(b + 2); // Curr v3
        
        // Right Cap
        indices.push_back(centerRightIdx);
        indices.push_back(b + 3); // Curr v4
        indices.push_back(b + 7); // Next v4
    }
}

// Helper for a detailed wheel with spokes
static void AddDetailedWheel(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                             const XMFLOAT3& center, float radius, float width)
{
    // Colors
    XMFLOAT4 rubberColor(0.2f, 0.2f, 0.2f, 1.0f); 
    XMFLOAT4 rimColor(0.65f, 0.65f, 0.65f, 1.0f); // Silver
    XMFLOAT4 spokeColor(0.6f, 0.6f, 0.6f, 1.0f); // Slightly darker silver

    // 1. Tire (Main Cylinder)
    AddCylinderToMesh(vertices, indices, center, radius, width, true, rubberColor);
    
    // 2. Rim (Outer Ring) - Slightly smaller radius, wider to stick out
    float rimRadius = radius * 0.65f;
    float rimWidth = width + 0.02f;
    AddCylinderToMesh(vertices, indices, center, rimRadius, rimWidth, true, rimColor);

    // 3. Hub (Center Cap)
    float hubRadius = radius * 0.15f;
    float hubWidth = width + 0.04f;
    AddCylinderToMesh(vertices, indices, center, hubRadius, hubWidth, true, rimColor);

    // 4. Spokes (Crossed Boxes)
    float spokeThick = radius * 0.08f;
    float spokeLen = rimRadius * 1.8f; // Go through center
    float spokeDepth = 0.03f; // Stick out from rim center
    
    // Determine X offset for spokes (put them on the outer face)
    float faceX = center.x + (center.x > 0 ? width/2 : -width/2) + (center.x > 0 ? 0.01f : -0.01f);
    
    // Vertical Spoke
    AddBoxToMesh(vertices, indices, XMFLOAT3(faceX, center.y, center.z), 
        XMFLOAT3(spokeDepth, rimRadius, spokeThick), spokeColor);
        
    // Horizontal Spoke (actually Z axis in vehicle space)
    AddBoxToMesh(vertices, indices, XMFLOAT3(faceX, center.y, center.z), 
        XMFLOAT3(spokeDepth, spokeThick, rimRadius), spokeColor);
}

// Helper for a detailed wheel with spokes wrapper
static void AddDetailedWheelWithSpokes(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                             const XMFLOAT3& center, float radius, float width)
{
    AddDetailedWheel(vertices, indices, center, radius, width);
}

// Helper to add a tapered box (Trapezoid Prism) - Used for cabin
static void AddTrapezoid(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                         const XMFLOAT3& center, const XMFLOAT3& bottomSize, 
                         float topScaleX, float topScaleZ,
                         const XMFLOAT4& color)
{
    float w = bottomSize.x; float h = bottomSize.y; float l = bottomSize.z;
    float tw = w * topScaleX; float tl = l * topScaleZ;
    
    XMFLOAT3 vPos[8] = {
        {center.x - w, center.y - h, center.z - l}, {center.x + w, center.y - h, center.z - l},
        {center.x + w, center.y - h, center.z + l}, {center.x - w, center.y - h, center.z + l},
        {center.x - tw, center.y + h, center.z - tl}, {center.x + tw, center.y + h, center.z - tl},
        {center.x + tw, center.y + h, center.z + tl}, {center.x - tw, center.y + h, center.z + tl}
    };
    
    // Simple auto-normals for simplicity (not perfect for smoothing but good for low poly)
    auto AddQuad = [&](int a, int b, int c, int d) {
        XMFLOAT3 p0 = vPos[a]; XMFLOAT3 p1 = vPos[b]; XMFLOAT3 p2 = vPos[c];
        XMVECTOR v1 = XMLoadFloat3(&p1) - XMLoadFloat3(&p0);
        XMVECTOR v2 = XMLoadFloat3(&p2) - XMLoadFloat3(&p0);
        XMFLOAT3 n; XMStoreFloat3(&n, XMVector3Normalize(XMVector3Cross(v2, v1))); // CCW
        
        int idx = (int)vertices.size();
        Vertex v; v.Normal = n; v.Color = color;
        v.Position = vPos[a]; vertices.push_back(v);
        v.Position = vPos[b]; vertices.push_back(v);
        v.Position = vPos[c]; vertices.push_back(v);
        v.Position = vPos[d]; vertices.push_back(v);
        
        indices.push_back(idx); indices.push_back(idx+1); indices.push_back(idx+2);
        indices.push_back(idx); indices.push_back(idx+2); indices.push_back(idx+3);
    };

    AddQuad(3, 2, 1, 0); // Bottom (Upside down?) - Normal points down
    AddQuad(4, 5, 6, 7); // Top
    AddQuad(0, 1, 5, 4); // Back
    AddQuad(1, 2, 6, 5); // Right
    AddQuad(2, 3, 7, 6); // Front
    AddQuad(3, 0, 4, 7); // Left
}

static void CreateVehicleMesh(VehicleType type,
                             std::vector<Vertex>& vertices,
                             std::vector<uint32_t>& indices,
                             const XMFLOAT4& paintColor)
{
    vertices.clear();
    indices.clear();

    // Palette
    XMFLOAT4 undercarriageColor(0.2f, 0.2f, 0.22f, 1.0f); // Dark Grey
    XMFLOAT4 bumperColor(0.15f, 0.15f, 0.15f, 1.0f); // Black Plastic
    XMFLOAT4 glassColor(0.2f, 0.5f, 0.8f, 0.9f); // Blue Glass
    XMFLOAT4 grillColor(0.1f, 0.1f, 0.1f, 1.0f); // Black Grill
    XMFLOAT4 lightWhite(1.0f, 0.98f, 0.9f, 1.0f); // Headlight
    XMFLOAT4 lightRed(0.8f, 0.1f, 0.1f, 1.0f); // Taillight
    XMFLOAT4 plateYellow(1.0f, 0.8f, 0.2f, 1.0f); // License Plate
    XMFLOAT4 handleColor(0.1f, 0.1f, 0.1f, 1.0f); // Black Handles
    XMFLOAT4 exhaustColor(0.5f, 0.5f, 0.5f, 1.0f); // Chrome/Steel

    // Base dimensions (Unit scale approx meters)
    float length = 4.6f; 
    float width = 1.9f;
    float chassisHeight = 0.4f; // Height of the dark bottom slab
    float bodyHeight = 0.55f;   // Height of the painted lower body
    float cabinHeight = 0.6f;   // Height of the greenhouse
    float wheelRadius = 0.36f;
    float wheelWidth = 0.25f;

    // Style Modifiers
    float hoodRatio = 0.35f; // % of length that is hood
    float trunkRatio = 0.20f; // % of length that is trunk
    float cabinSlope = 0.85f; // 1.0 = boxy, <1.0 = tapered
    bool hasSeparateBed = false; // For truck

    if (type == VehicleType::SUV) {
        length = 4.9f; width = 2.1f; 
        chassisHeight = 0.5f; bodyHeight = 0.65f; cabinHeight = 0.7f;
        hoodRatio = 0.3f; trunkRatio = 0.05f; // SUV has minimal trunk deck
        cabinSlope = 0.95f; // Boxy
        wheelRadius = 0.43f;
    } else if (type == VehicleType::Truck) {
        length = 5.5f; width = 2.2f;
        chassisHeight = 0.6f; bodyHeight = 0.6f; cabinHeight = 0.75f;
        hoodRatio = 0.3f; trunkRatio = 0.35f; // Long bed
        hasSeparateBed = true;
        wheelRadius = 0.46f;
    } else if (type == VehicleType::SportsCar) {
        length = 4.5f; width = 2.0f;
        chassisHeight = 0.3f; bodyHeight = 0.45f; cabinHeight = 0.45f;
        hoodRatio = 0.42f; trunkRatio = 0.15f;
        cabinSlope = 0.55f; // Very streamlined
        wheelRadius = 0.39f;
    } else if (type == VehicleType::Bus) {
        length = 9.5f; width = 2.6f;
        chassisHeight = 0.5f; bodyHeight = 1.0f; cabinHeight = 1.5f;
        hoodRatio = 0.0f; trunkRatio = 0.0f; // Flat front/back (no distinct hood/trunk)
        cabinSlope = 1.0f; // Boxy
        wheelRadius = 0.55f;
    }

    // Scale factor to match scene size
    float S = 0.6f; 
    
    // --- 1. Chassis (Undercarriage) ---
    // Solid slab at the bottom, raised slightly for ground clearance
    float groundClearance = wheelRadius * 0.6f;
    XMFLOAT3 chassisCenter(0, (groundClearance + chassisHeight/2)*S, 0);
    XMFLOAT3 chassisSize((width/2)*S, (chassisHeight/2)*S, (length/2)*S);
    AddBoxToMesh(vertices, indices, chassisCenter, chassisSize, undercarriageColor);

    // --- 2. Lower Body (The Painted Part) ---
    float lbY = (groundClearance + chassisHeight + bodyHeight/2) * S;
    float lbW = (width/2) * S * 1.02f; // Slightly wider than chassis
    
    float hoodLen = length * hoodRatio; 

    if (type == VehicleType::Bus) {
        // Bus is one big block
        AddBoxToMesh(vertices, indices, 
            XMFLOAT3(0, lbY + (cabinHeight/2)*S, 0), 
            XMFLOAT3(lbW, ((bodyHeight+cabinHeight)/2)*S, (length/2)*S), 
            paintColor);

        // Bus Greenhouse (Windows are part of the main body for simplicity)
        // Main windshield
        float busWindshieldH = cabinHeight * S * 0.5f;
        float busWindshieldW = (width/2) * S * 0.9f;
        float busWindshieldZ = (length/2) * S + 0.01f*S; // Slightly in front
        AddBoxToMesh(vertices, indices, XMFLOAT3(0, lbY + (bodyHeight/2)*S + busWindshieldH/2, busWindshieldZ),
                     XMFLOAT3(busWindshieldW, busWindshieldH/2, 0.01f*S), glassColor);
        // Side windows (simplified as large boxes)
        float sideWindowL = (length/2 - 0.1f) * S;
        float sideWindowH = cabinHeight * S * 0.7f;
        float sideWindowY = lbY + (bodyHeight/2)*S + sideWindowH/2;
        float sideWindowX = (width/2) * S * 0.9f; // Inside body
        AddBoxToMesh(vertices, indices, XMFLOAT3(sideWindowX, sideWindowY, 0), 
                     XMFLOAT3(0.01f*S, sideWindowH/2, sideWindowL), glassColor);
        AddBoxToMesh(vertices, indices, XMFLOAT3(-sideWindowX, sideWindowY, 0), 
                     XMFLOAT3(0.01f*S, sideWindowH/2, sideWindowL), glassColor);

    } else {
        // Three-Box Design
        float trunkLen = length * trunkRatio;
        float cabinLen = length - hoodLen - trunkLen;
        
        float zHood = (length/2 - hoodLen/2) * S;
        float zCabin = (length/2 - hoodLen - cabinLen/2) * S;
        float zTrunk = (-length/2 + trunkLen/2) * S;

        // Hood Segment
        AddBoxToMesh(vertices, indices, XMFLOAT3(0, lbY, zHood), XMFLOAT3(lbW, (bodyHeight/2)*S, (hoodLen/2)*S), paintColor);
        
        // Main Cabin Base
        AddBoxToMesh(vertices, indices, XMFLOAT3(0, lbY, zCabin), XMFLOAT3(lbW, (bodyHeight/2)*S, (cabinLen/2)*S), paintColor);
        
        // Trunk/Bed Segment
        if (hasSeparateBed && type == VehicleType::Truck) {
             // Bed Floor
             AddBoxToMesh(vertices, indices, XMFLOAT3(0, lbY - (bodyHeight*0.2f)*S, zTrunk), 
                XMFLOAT3(lbW, (bodyHeight*0.3f)*S, (trunkLen/2)*S), paintColor);
             // Bed Sides (Left/Right)
             float wallThick = 0.1f * S;
             AddBoxToMesh(vertices, indices, XMFLOAT3(-lbW + wallThick, lbY + (0.15f*S), zTrunk), 
                XMFLOAT3(wallThick, (bodyHeight/2)*S, (trunkLen/2)*S), paintColor);
             AddBoxToMesh(vertices, indices, XMFLOAT3(lbW - wallThick, lbY + (0.15f*S), zTrunk), 
                XMFLOAT3(wallThick, (bodyHeight/2)*S, (trunkLen/2)*S), paintColor);
             // Tailgate
             AddBoxToMesh(vertices, indices, XMFLOAT3(0, lbY + (0.15f*S), zTrunk - (trunkLen/2)*S + wallThick), 
                XMFLOAT3(lbW, (bodyHeight/2)*S, wallThick), paintColor);
        } else {
            AddBoxToMesh(vertices, indices, XMFLOAT3(0, lbY, zTrunk), XMFLOAT3(lbW, (bodyHeight/2)*S, (trunkLen/2)*S), paintColor);
        }

        // --- 3. Greenhouse (Upper Body) ---
        float ghY = lbY + (bodyHeight/2 + cabinHeight/2) * S;
        float ghBaseW = lbW * 0.9f;
        float ghBaseL = (cabinLen/2) * S;
        
        // Pillars/Roof (Trapezoid)
        AddTrapezoid(vertices, indices, 
            XMFLOAT3(0, ghY, zCabin), 
            XMFLOAT3(ghBaseW, (cabinHeight/2)*S, ghBaseL), 
            0.85f, cabinSlope, paintColor);

        // Glass (Inset Trapezoid)
        AddTrapezoid(vertices, indices, 
            XMFLOAT3(0, ghY, zCabin), 
            XMFLOAT3(ghBaseW * 0.98f, (cabinHeight/2)*S * 0.96f, ghBaseL * 0.98f), 
            0.85f, cabinSlope, glassColor);
            
        // Roof Rails (SUV)
        if (type == VehicleType::SUV) {
            float railX = ghBaseW * 0.7f;
            float railY = ghY + (cabinHeight/2)*S + 0.05f*S;
            AddBoxToMesh(vertices, indices, XMFLOAT3(railX, railY, zCabin), 
                XMFLOAT3(0.05f*S, 0.04f*S, ghBaseL*0.8f), bumperColor);
            AddBoxToMesh(vertices, indices, XMFLOAT3(-railX, railY, zCabin), 
                XMFLOAT3(0.05f*S, 0.04f*S, ghBaseL*0.8f), bumperColor);
        }
        
        // Spoiler (SportsCar)
        if (type == VehicleType::SportsCar) {
            float spoilZ = (-length/2 + trunkLen*0.2f) * S;
            float spoilY = lbY + (bodyHeight/2)*S + 0.15f*S;
            // Wing
            AddBoxToMesh(vertices, indices, XMFLOAT3(0, spoilY, spoilZ), 
                XMFLOAT3(lbW, 0.02f*S, 0.2f*S), paintColor);
            // Struts
            AddBoxToMesh(vertices, indices, XMFLOAT3(lbW*0.6f, spoilY-0.1f*S, spoilZ), 
                XMFLOAT3(0.05f*S, 0.1f*S, 0.05f*S), paintColor);
            AddBoxToMesh(vertices, indices, XMFLOAT3(-lbW*0.6f, spoilY-0.1f*S, spoilZ), 
                XMFLOAT3(0.05f*S, 0.1f*S, 0.05f*S), paintColor);
        }
    }

    // --- 4. Wheels & Wells ---
    float wheelZFront = (length/2 - length*0.18f) * S;
    float wheelZRear = (-length/2 + length*0.18f) * S;
    float wY = wheelRadius * S;
    float wX = (width/2) * S; 

    auto AddWheelSet = [&](float z) {
        // Wheel Well (Black interior)
        XMFLOAT3 wellPosL(-wX, wY, z);
        XMFLOAT3 wellPosR(wX, wY, z);
        // A slightly larger cylinder for the well
        AddCylinderToMesh(vertices, indices, wellPosL, wheelRadius*S*1.15f, wheelWidth*S*1.5f, true, XMFLOAT4(0.1f,0.1f,0.1f,1.0f));
        AddCylinderToMesh(vertices, indices, wellPosR, wheelRadius*S*1.15f, wheelWidth*S*1.5f, true, XMFLOAT4(0.1f,0.1f,0.1f,1.0f));
        
        // Wheel (Detailed)
        float wheelOut = 0.06f * S;
        AddDetailedWheelWithSpokes(vertices, indices, XMFLOAT3(-(wX+wheelOut), wY, z), wheelRadius*S, wheelWidth*S);
        AddDetailedWheelWithSpokes(vertices, indices, XMFLOAT3((wX+wheelOut), wY, z), wheelRadius*S, wheelWidth*S);
        
        // Fender Flares
        if (type != VehicleType::Bus) {
            float flareH = 0.08f * S;
            float flareW = 0.12f * S;
            float flareL = wheelRadius * S * 2.2f;
            float flareY = wY + wheelRadius*S*0.8f;
            
            AddBoxToMesh(vertices, indices, XMFLOAT3(-(wX+wheelWidth*S), flareY, z), 
                XMFLOAT3(flareW, flareH, flareL/2), paintColor);
            AddBoxToMesh(vertices, indices, XMFLOAT3((wX+wheelWidth*S), flareY, z), 
                XMFLOAT3(flareW, flareH, flareL/2), paintColor);
        }
    };

    AddWheelSet(wheelZFront);
    AddWheelSet(wheelZRear);

    // --- 5. Details ---
    
    // Grill (Front)
    float frontZ = (length/2) * S;
    float grillY = lbY;
    if (type != VehicleType::Bus) {
        AddBoxToMesh(vertices, indices, XMFLOAT3(0, grillY, frontZ + 0.02f*S), 
            XMFLOAT3(lbW*0.7f, (bodyHeight/3)*S, 0.05f*S), grillColor);
    }
    
    // Headlights
    float lightY = grillY + (bodyHeight/4)*S;
    float lightX = lbW * 0.7f;
    AddBoxToMesh(vertices, indices, XMFLOAT3(lightX, lightY, frontZ + 0.01f*S), 
        XMFLOAT3(0.15f*S, 0.08f*S, 0.06f*S), lightWhite);
    AddBoxToMesh(vertices, indices, XMFLOAT3(-lightX, lightY, frontZ + 0.01f*S), 
        XMFLOAT3(0.15f*S, 0.08f*S, 0.06f*S), lightWhite);

    // Taillights
    float backZ = (-length/2) * S;
    AddBoxToMesh(vertices, indices, XMFLOAT3(lightX, lightY, backZ - 0.01f*S), 
        XMFLOAT3(0.15f*S, 0.08f*S, 0.06f*S), lightRed);
    AddBoxToMesh(vertices, indices, XMFLOAT3(-lightX, lightY, backZ - 0.01f*S), 
        XMFLOAT3(0.15f*S, 0.08f*S, 0.06f*S), lightRed);
    
    // Bumpers
    float bumperY = chassisCenter.y;
    AddBoxToMesh(vertices, indices, XMFLOAT3(0, bumperY, frontZ + 0.08f*S), 
        XMFLOAT3(lbW, 0.12f*S, 0.1f*S), bumperColor);
    AddBoxToMesh(vertices, indices, XMFLOAT3(0, bumperY, backZ - 0.08f*S), 
        XMFLOAT3(lbW, 0.12f*S, 0.1f*S), bumperColor);

    // License Plates
    float plateY = bumperY;
    // Front
    AddBoxToMesh(vertices, indices, XMFLOAT3(0, plateY, frontZ + 0.19f*S), 
        XMFLOAT3(0.25f*S, 0.08f*S, 0.01f*S), plateYellow);
    // Rear
    AddBoxToMesh(vertices, indices, XMFLOAT3(0, plateY, backZ - 0.19f*S), 
        XMFLOAT3(0.25f*S, 0.08f*S, 0.01f*S), plateYellow);
        
    // Exhaust Pipes (Rear, Bottom)
    if (type != VehicleType::Bus) {
        float exX = lbW * 0.6f;
        float exY = bumperY - 0.1f*S;
        float exZ = backZ - 0.05f*S;
        AddCylinderToMesh(vertices, indices, XMFLOAT3(exX, exY, exZ), 0.04f*S, 0.2f*S, false, exhaustColor);
        AddCylinderToMesh(vertices, indices, XMFLOAT3(-exX, exY, exZ), 0.04f*S, 0.2f*S, false, exhaustColor);
    }
    
    // Door Handles
    if (type != VehicleType::Bus) {
        float handleZ = 0.0f; // Center
        float handleY = lbY + (bodyHeight/2)*S - 0.05f*S;
        float handleX = lbW + 0.02f*S;
        AddBoxToMesh(vertices, indices, XMFLOAT3(handleX, handleY, handleZ), 
            XMFLOAT3(0.02f*S, 0.03f*S, 0.08f*S), handleColor);
        AddBoxToMesh(vertices, indices, XMFLOAT3(-handleX, handleY, handleZ), 
            XMFLOAT3(0.02f*S, 0.03f*S, 0.08f*S), handleColor);
    }
    
    // Side Mirrors
    if (type != VehicleType::Bus) {
        float mirrorZ = (length/2 - hoodLen - 0.2f) * S;
        float mirrorY = lbY + (bodyHeight/2)*S;
        float mirrorX = lbW + 0.1f*S;
        AddBoxToMesh(vertices, indices, XMFLOAT3(mirrorX, mirrorY, mirrorZ), XMFLOAT3(0.1f*S, 0.08f*S, 0.05f*S), paintColor);
        AddBoxToMesh(vertices, indices, XMFLOAT3(-mirrorX, mirrorY, mirrorZ), XMFLOAT3(0.1f*S, 0.08f*S, 0.05f*S), paintColor);
    }
}

void Scene::AddVehicle(const std::string& name, VehicleType type,
                       VehicleDirection direction, MovementIntention intention,
                       float speed)
{
    // Create vehicle mesh
    // Create vehicle mesh
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Pick a random color for the vehicle body
    XMFLOAT4 bodyColor;
    switch (type) {
        case VehicleType::Sedan: bodyColor = XMFLOAT4(0.8f, 0.2f, 0.2f, 1.0f); break; // Red
        case VehicleType::SUV: bodyColor = XMFLOAT4(0.2f, 0.8f, 0.2f, 1.0f); break; // Green
        case VehicleType::Truck: bodyColor = XMFLOAT4(0.2f, 0.4f, 0.8f, 1.0f); break; // Blue
        case VehicleType::Bus: bodyColor = XMFLOAT4(0.9f, 0.6f, 0.1f, 1.0f); break; // Orange
        case VehicleType::SportsCar: bodyColor = XMFLOAT4(0.9f, 0.9f, 0.1f, 1.0f); break; // Yellow
        default: bodyColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f); break;
    }
    
    CreateVehicleMesh(type, vertices, indices, bodyColor);

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
        obj->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);  // White (use vertex colors)
        break;
    case VehicleType::SUV:
        obj->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        break;
    case VehicleType::Truck:
        obj->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        break;
    case VehicleType::Bus:
        obj->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        break;
    case VehicleType::SportsCar:
        obj->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        break;
    }

    // Create mesh in renderer
    if (m_renderer)
    {
        Mesh mesh = Renderer::CreateMeshFromData(vertices, indices);
        m_renderer->UploadMesh(mesh);
        obj->data.customMesh.mesh = new Mesh(std::move(mesh));

        // Create FCL collision geometry using full mesh
        if (m_driver && m_driver->IsConnected())
        {
            // Extract positions for FCL driver (which expects XMFLOAT3)
            std::vector<XMFLOAT3> positions;
            positions.reserve(vertices.size());
            for (const auto& v : vertices)
            {
                positions.push_back(v.Position);
            }
            obj->fclHandle = m_driver->CreateMesh(positions, indices);
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
        Mesh mesh = Renderer::CreateMeshFromData(meshData.vertices, meshData.indices);
        m_renderer->UploadMesh(mesh);
        obj->data.customMesh.mesh = new Mesh(std::move(mesh));

        // Create FCL collision geometry using full mesh
        if (m_driver && m_driver->IsConnected())
        {
            obj->fclHandle = m_driver->CreateMesh(meshData.vertices, meshData.indices);
        }
    }

    m_objects.push_back(std::move(obj));
}

bool Scene::SaveScene(const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
        return false;

    try
    {
        // Save header
        file << "FCL_SCENE_V1\n";
        file << "ObjectCount: " << m_objects.size() << "\n";
        file << "\n";

        // Save each object (only basic shapes - spheres and boxes)
        for (const auto& obj : m_objects)
        {
            // Skip complex objects (vehicles, meshes, etc.) for simplicity
            if (obj->type == GeometryType::Mesh || obj->isVehicle || obj->isOrbiting)
                continue;

            file << "OBJECT\n";
            file << "Name: " << obj->name << "\n";
            file << "Type: " << (obj->type == GeometryType::Sphere ? "Sphere" : "Box") << "\n";
            file << "Position: " << obj->position.x << " " << obj->position.y << " " << obj->position.z << "\n";
            file << "Rotation: " << obj->rotation.x << " " << obj->rotation.y << " " << obj->rotation.z << "\n";
            file << "Color: " << obj->color.x << " " << obj->color.y << " " << obj->color.z << " " << obj->color.w << "\n";

            if (obj->type == GeometryType::Sphere)
            {
                file << "Radius: " << obj->data.sphere.radius << "\n";
            }
            else if (obj->type == GeometryType::Box)
            {
                file << "Extents: " << obj->data.box.extents.x << " "
                     << obj->data.box.extents.y << " "
                     << obj->data.box.extents.z << "\n";
            }

            // Velocity if present
            if (obj->hasVelocity)
            {
                file << "Velocity: " << obj->velocity.x << " " << obj->velocity.y << " " << obj->velocity.z << "\n";
            }

            file << "END_OBJECT\n\n";
        }

        file.close();
        return true;
    }
    catch (...)
    {
        file.close();
        return false;
    }
}

bool Scene::LoadScene(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
        return false;

    try
    {
        std::string line;

        // Read header
        std::getline(file, line);
        if (line != "FCL_SCENE_V1")
            return false;

        // Clear current scene
        ClearAllObjects();

        // Read object count
        std::getline(file, line); // ObjectCount line
        std::getline(file, line); // Empty line

        // Read objects
        while (std::getline(file, line))
        {
            if (line == "OBJECT")
            {
                std::string name, type;
                XMFLOAT3 position(0, 0, 0), rotation(0, 0, 0);
                XMFLOAT4 color(0.7f, 0.7f, 0.7f, 1.0f);
                float radius = 1.0f;
                XMFLOAT3 extents(1, 1, 1);
                XMFLOAT3 velocity(0, 0, 0);
                bool hasVelocity = false;

                // Read object properties
                while (std::getline(file, line) && line != "END_OBJECT")
                {
                    if (line.substr(0, 6) == "Name: ")
                    {
                        name = line.substr(6);
                    }
                    else if (line.substr(0, 6) == "Type: ")
                    {
                        type = line.substr(6);
                    }
                    else if (line.substr(0, 10) == "Position: ")
                    {
                        sscanf_s(line.c_str() + 10, "%f %f %f", &position.x, &position.y, &position.z);
                    }
                    else if (line.substr(0, 10) == "Rotation: ")
                    {
                        sscanf_s(line.c_str() + 10, "%f %f %f", &rotation.x, &rotation.y, &rotation.z);
                    }
                    else if (line.substr(0, 7) == "Color: ")
                    {
                        sscanf_s(line.c_str() + 7, "%f %f %f %f", &color.x, &color.y, &color.z, &color.w);
                    }
                    else if (line.substr(0, 8) == "Radius: ")
                    {
                        sscanf_s(line.c_str() + 8, "%f", &radius);
                    }
                    else if (line.substr(0, 9) == "Extents: ")
                    {
                        sscanf_s(line.c_str() + 9, "%f %f %f", &extents.x, &extents.y, &extents.z);
                    }
                    else if (line.substr(0, 10) == "Velocity: ")
                    {
                        sscanf_s(line.c_str() + 10, "%f %f %f", &velocity.x, &velocity.y, &velocity.z);
                        hasVelocity = true;
                    }
                }

                // Create object based on type
                if (type == "Sphere")
                {
                    if (hasVelocity)
                    {
                        AddAsteroid(name, position, velocity, radius);
                    }
                    else
                    {
                        AddSphere(name, position, radius);
                    }
                }
                else if (type == "Box")
                {
                    AddBox(name, position, extents);
                }

                // Set color and rotation
                if (!m_objects.empty())
                {
                    auto& obj = m_objects.back();
                    obj->color = color;
                    obj->rotation = rotation;
                }
            }
        }

        file.close();
        return true;
    }
    catch (...)
    {
        file.close();
        return false;
    }
}
