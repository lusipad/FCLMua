#pragma once
#include <directxmath.h>

using namespace DirectX;

class Camera
{
public:
    Camera();

    // Camera controls
    void Rotate(float deltaYaw, float deltaPitch);
    void Pan(float deltaX, float deltaY);
    void Zoom(float delta);

    // Getters
    XMMATRIX GetViewMatrix() const;
    XMMATRIX GetProjectionMatrix(float aspectRatio) const;
    XMFLOAT3 GetPosition() const;
    XMFLOAT3 GetTarget() const { return m_target; }
    float GetDistance() const { return m_distance; }

    // Setters
    void SetTarget(const XMFLOAT3& target) { m_target = target; }
    void SetDistance(float distance) { m_distance = distance; }
    void SetYaw(float yaw) { m_yaw = yaw; }
    void SetPitch(float pitch) { m_pitch = pitch; }

private:
    XMFLOAT3 m_target;  // Look-at target
    float m_distance;   // Distance from target
    float m_yaw;        // Rotation around Y axis (radians)
    float m_pitch;      // Rotation around X axis (radians)
    float m_fov;        // Field of view (radians)
    float m_nearPlane;
    float m_farPlane;
};
