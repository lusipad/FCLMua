#pragma once

#include <memory>

#include <fcl/geometry/collision_geometry.h>
#include <fcl/common/types.h>

#include "fclmusa/geometry.h"

namespace fclmusa::upstream {

struct GeometryBinding {
    std::shared_ptr<fcl::CollisionGeometryd> Geometry;
    FCL_TRANSFORM LocalTransform;
};

struct CollisionObjects {
    GeometryBinding Object1;
    GeometryBinding Object2;
};

FCL_TRANSFORM
CombineTransforms(
    const FCL_TRANSFORM& parent,
    const FCL_TRANSFORM& child) noexcept;

fcl::Transform3d
ToEigenTransform(
    const FCL_TRANSFORM& transform) noexcept;

NTSTATUS
BuildGeometryBinding(
    const FCL_GEOMETRY_SNAPSHOT& snapshot,
    GeometryBinding* binding) noexcept;

NTSTATUS
BuildCollisionObjects(
    const FCL_GEOMETRY_SNAPSHOT& object1,
    const FCL_TRANSFORM& transform1,
    const FCL_GEOMETRY_SNAPSHOT& object2,
    const FCL_TRANSFORM& transform2,
    CollisionObjects* objects,
    fcl::Transform3d& tf1,
    fcl::Transform3d& tf2) noexcept;

}  // namespace fclmusa::upstream
