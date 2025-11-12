#pragma once

#include "fclmusa/geometry/math_utils.h"

namespace fclmusa::geom {

struct OrientedBox {
    FCL_VECTOR3 Center;
    FCL_VECTOR3 Axes[3];
    FCL_VECTOR3 Extents;
};

inline OrientedBox BuildWorldObb(const FCL_OBB_GEOMETRY_DESC& desc, const FCL_TRANSFORM& transform) noexcept {
    OrientedBox box = {};
    const FCL_MATRIX3X3 rotation = MultiplyMatrix(transform.Rotation, desc.Rotation);
    box.Center = TransformPoint(transform, desc.Center);
    for (int axis = 0; axis < 3; ++axis) {
        FCL_VECTOR3 vec = {rotation.M[0][axis], rotation.M[1][axis], rotation.M[2][axis]};
        box.Axes[axis] = Normalize(vec);
        if (Length(box.Axes[axis]) <= kSingularityEpsilon) {
            box.Axes[axis] = {0.0f, 0.0f, 0.0f};
        }
    }
    box.Extents = desc.Extents;
    return box;
}

inline FCL_VECTOR3 ClosestPointOnObb(const OrientedBox& box, const FCL_VECTOR3& point) noexcept {
    FCL_VECTOR3 result = box.Center;
    const FCL_VECTOR3 delta = Subtract(point, box.Center);
    for (int i = 0; i < 3; ++i) {
        const float distance = Dot(delta, box.Axes[i]);
        const float extent = (&box.Extents.X)[i];
        const float clamped = Clamp(distance, -extent, extent);
        result = Add(result, Scale(box.Axes[i], clamped));
    }
    return result;
}

inline FCL_VECTOR3 SupportPoint(const OrientedBox& box, const FCL_VECTOR3& direction) noexcept {
    FCL_VECTOR3 result = box.Center;
    for (int i = 0; i < 3; ++i) {
        const float extent = (&box.Extents.X)[i];
        const float sign = Dot(direction, box.Axes[i]) >= 0.0f ? 1.0f : -1.0f;
        result = Add(result, Scale(box.Axes[i], sign * extent));
    }
    return result;
}

}  // namespace fclmusa::geom
