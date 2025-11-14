#include "camera.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera::Camera()
    : m_target(0.0f, 0.0f, 0.0f)
    , m_distance(10.0f)
    , m_yaw(static_cast<float>(M_PI) * 0.25f)
    , m_pitch(static_cast<float>(M_PI) * 0.15f)
    , m_fov(static_cast<float>(M_PI) / 4.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(1000.0f)
{
}

void Camera::Rotate(float deltaYaw, float deltaPitch)
{
    m_yaw += deltaYaw;
    m_pitch += deltaPitch;

    // Clamp pitch to avoid gimbal lock
    m_pitch = std::max(-static_cast<float>(M_PI) / 2.0f + 0.1f,
                       std::min(static_cast<float>(M_PI) / 2.0f - 0.1f, m_pitch));
}

void Camera::Pan(float deltaX, float deltaY)
{
    // Calculate right and up vectors
    XMVECTOR eye = XMLoadFloat3(&GetPosition());
    XMVECTOR target = XMLoadFloat3(&m_target);
    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, eye));
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, worldUp));
    XMVECTOR up = XMVector3Cross(right, forward);

    // Move target
    XMVECTOR offset = XMVectorAdd(
        XMVectorScale(right, deltaX),
        XMVectorScale(up, deltaY)
    );

    XMFLOAT3 offsetFloat;
    XMStoreFloat3(&offsetFloat, offset);

    m_target.x += offsetFloat.x;
    m_target.y += offsetFloat.y;
    m_target.z += offsetFloat.z;
}

void Camera::Zoom(float delta)
{
    m_distance -= delta;
    m_distance = std::max(1.0f, std::min(100.0f, m_distance));
}

XMMATRIX Camera::GetViewMatrix() const
{
    XMFLOAT3 position = GetPosition();
    XMVECTOR eye = XMLoadFloat3(&position);
    XMVECTOR target = XMLoadFloat3(&m_target);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return XMMatrixLookAtLH(eye, target, up);
}

XMMATRIX Camera::GetProjectionMatrix(float aspectRatio) const
{
    return XMMatrixPerspectiveFovLH(m_fov, aspectRatio, m_nearPlane, m_farPlane);
}

XMFLOAT3 Camera::GetPosition() const
{
    // Calculate position from spherical coordinates
    float x = m_target.x + m_distance * cos(m_pitch) * cos(m_yaw);
    float y = m_target.y + m_distance * sin(m_pitch);
    float z = m_target.z + m_distance * cos(m_pitch) * sin(m_yaw);
    return XMFLOAT3(x, y, z);
}
