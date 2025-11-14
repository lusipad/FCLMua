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

// Scene mode
enum class SceneMode
{
    Default,
    SolarSystem,
    CrossroadSimulation
};

// Geometry types
enum class GeometryType
{
    Sphere,
    Box,
    Mesh
};

// Vehicle direction (for crossroad scene)
enum class VehicleDirection
{
    North,  // Moving from south to north
    South,  // Moving from north to south
    East,   // Moving from west to east
    West    // Moving from east to west
};

// Vehicle movement intention (for crossroad scene)
enum class MovementIntention
{
    GoStraight,
    TurnLeft,
    TurnRight
};

// Vehicle type (for different meshes in crossroad scene)
enum class VehicleType
{
    Sedan,      // Regular car
    SUV,        // Larger car
    Truck,      // Truck
    Bus,        // Bus
    SportsCar   // Sports car
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

    // Orbital motion data (for solar system scene)
    bool isOrbiting;
    float orbitalRadius;      // Distance from orbit center
    float orbitalSpeed;       // Radians per second
    float currentAngle;       // Current angle in orbit
    XMFLOAT3 orbitCenter;     // Point to orbit around

    // Physics data (for asteroids)
    bool hasVelocity;
    XMFLOAT3 velocity;        // Linear velocity

    // Vehicle data (for crossroad scene)
    bool isVehicle;                      // Whether this object is a vehicle
    VehicleType vehicleType;             // Type of vehicle (for mesh selection)
    VehicleDirection vehicleDirection;   // Current travel direction
    MovementIntention movementIntention; // What the vehicle intends to do at crossroad
    float vehicleSpeed;                  // Speed in units per second
    bool hasCrossedIntersection;         // Whether vehicle has crossed the crossroad line
    XMFLOAT3 targetPosition;             // Next waypoint to move towards
    float distanceTraveled;              // Distance traveled on current path

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
        , isOrbiting(false)
        , orbitalRadius(0)
        , orbitalSpeed(0)
        , currentAngle(0)
        , orbitCenter(0, 0, 0)
        , hasVelocity(false)
        , velocity(0, 0, 0)
        , isVehicle(false)
        , vehicleType(VehicleType::Sedan)
        , vehicleDirection(VehicleDirection::North)
        , movementIntention(MovementIntention::GoStraight)
        , vehicleSpeed(0)
        , hasCrossedIntersection(false)
        , targetPosition(0, 0, 0)
        , distanceTraveled(0)
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

    // Scene mode management
    void SetSceneMode(SceneMode mode);
    SceneMode GetSceneMode() const { return m_sceneMode; }
    void InitializeDefaultScene();
    void InitializeSolarSystem();
    void InitializeCrossroadScene();

    // Object management
    void AddSphere(const std::string& name, const XMFLOAT3& position, float radius);
    void AddBox(const std::string& name, const XMFLOAT3& position, const XMFLOAT3& extents);
    void AddMesh(const std::string& name, const XMFLOAT3& position,
                 const std::vector<XMFLOAT3>& vertices,
                 const std::vector<uint32_t>& indices);
    void DeleteObject(size_t index);
    void ClearAllObjects();

    // Asteroid management (for solar system scene)
    void AddAsteroid(const std::string& name, const XMFLOAT3& position,
                     const XMFLOAT3& velocity, float radius);
    void SetAsteroidVelocity(size_t index, const XMFLOAT3& velocity);

    // Vehicle management (for crossroad scene)
    void AddVehicle(const std::string& name, VehicleType type,
                   VehicleDirection direction, MovementIntention intention,
                   float speed);

    // Load vehicle from OBJ file
    void AddVehicleFromOBJ(const std::string& name, const std::string& objFilePath,
                          VehicleDirection direction, MovementIntention intention,
                          float speed, float scale = 1.0f);

    // Selection and transformation
    void SelectObject(size_t index);
    void DeselectAll();
    SceneObject* GetSelectedObject();
    size_t GetSelectedObjectIndex() const { return m_selectedObjectIndex; }

    // Collision detection
    void DetectCollisions();

    // Simulation speed control
    void SetSimulationSpeed(float speed) { m_simulationSpeed = speed; }
    float GetSimulationSpeed() const { return m_simulationSpeed; }

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
    void RenderOrbits();

    // Update orbital positions
    void UpdateOrbitalMotion(float deltaTime);
    void UpdatePhysics(float deltaTime);
    void UpdateVehicleMovement(float deltaTime);

    Renderer* m_renderer;
    FclDriver* m_driver;
    Camera m_camera;

    std::vector<std::unique_ptr<SceneObject>> m_objects;
    size_t m_selectedObjectIndex;

    // Scene mode
    SceneMode m_sceneMode;
    float m_simulationSpeed;

    // Input state
    bool m_isDragging;
    bool m_isPanning;
    bool m_isRotatingCamera;
    int m_lastMouseX;
    int m_lastMouseY;

    // Asteroid creation state (for solar system scene)
    bool m_isCreatingAsteroid;
    XMFLOAT3 m_asteroidVelocity;
};
