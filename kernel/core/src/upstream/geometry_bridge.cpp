#include "fclmusa/upstream/geometry_bridge.h"

#include <vector>

#include <fcl/geometry/bvh/BVH_model.h>
#include <fcl/geometry/shape/box.h>
#include <fcl/geometry/shape/sphere.h>

#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/memory/dpc_allocator.h"

namespace fclmusa::upstream {

namespace {

using GeometryPtr = std::shared_ptr<fcl::CollisionGeometryd>;
using BVHModeld = fcl::BVHModel<fcl::OBBRSS<double>>;
using fclmusa::memory::FclDpcNonPagedAllocator;

fcl::Matrix3d ToEigenMatrix(const FCL_MATRIX3X3& matrix) noexcept {
    fcl::Matrix3d result;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            result(row, col) = static_cast<double>(matrix.M[row][col]);
        }
    }
    return result;
}

fcl::Vector3d ToEigenVector(const FCL_VECTOR3& vector) noexcept {
    return fcl::Vector3d(
        static_cast<double>(vector.X),
        static_cast<double>(vector.Y),
        static_cast<double>(vector.Z));
}

NTSTATUS BuildSphereBinding(
    const FCL_SPHERE_GEOMETRY_DESC& desc,
    GeometryBinding* binding) noexcept {
    if (desc.Radius <= 0.0f || binding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    try {
        binding->Geometry = std::allocate_shared<fcl::Sphered>(
            FclDpcNonPagedAllocator<fcl::Sphered>{},
            static_cast<double>(desc.Radius));
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    binding->LocalTransform = fclmusa::geom::IdentityTransform();
    binding->LocalTransform.Translation = desc.Center;
    return STATUS_SUCCESS;
}

NTSTATUS BuildObbBinding(
    const FCL_OBB_GEOMETRY_DESC& desc,
    GeometryBinding* binding) noexcept {
    if (binding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    if (desc.Extents.X <= 0.0f || desc.Extents.Y <= 0.0f || desc.Extents.Z <= 0.0f) {
        return STATUS_INVALID_PARAMETER;
    }

    try {
        binding->Geometry = std::allocate_shared<fcl::Boxd>(
            FclDpcNonPagedAllocator<fcl::Boxd>{},
            static_cast<double>(desc.Extents.X * 2.0f),
            static_cast<double>(desc.Extents.Y * 2.0f),
            static_cast<double>(desc.Extents.Z * 2.0f));
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    binding->LocalTransform = fclmusa::geom::IdentityTransform();
    binding->LocalTransform.Rotation = desc.Rotation;
    binding->LocalTransform.Translation = desc.Center;
    return STATUS_SUCCESS;
}

NTSTATUS BuildMeshGeometry(
    const FCL_GEOMETRY_SNAPSHOT& snapshot,
    GeometryPtr* geometry) noexcept {
    if (geometry == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    const auto& mesh = snapshot.Data.Mesh;
    if (mesh.Vertices == nullptr || mesh.Indices == nullptr ||
        mesh.VertexCount == 0 || mesh.IndexCount < 3 || (mesh.IndexCount % 3) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    const ULONG triangleCount = mesh.IndexCount / 3;

    try {
        auto model = std::allocate_shared<BVHModeld>(FclDpcNonPagedAllocator<BVHModeld>{});
        std::vector<fcl::Vector3d> vertices(mesh.VertexCount);
        for (ULONG i = 0; i < mesh.VertexCount; ++i) {
            vertices[i] = ToEigenVector(mesh.Vertices[i]);
        }

        std::vector<fcl::Triangle> triangles(triangleCount);
        for (ULONG tri = 0; tri < triangleCount; ++tri) {
            const UINT32 i0 = mesh.Indices[tri * 3];
            const UINT32 i1 = mesh.Indices[tri * 3 + 1];
            const UINT32 i2 = mesh.Indices[tri * 3 + 2];
            if (i0 >= mesh.VertexCount || i1 >= mesh.VertexCount || i2 >= mesh.VertexCount) {
                return STATUS_INVALID_PARAMETER;
            }
            triangles[tri] = fcl::Triangle(i0, i1, i2);
        }

        model->beginModel(
            static_cast<int>(mesh.VertexCount),
            static_cast<int>(triangleCount));
        model->addSubModel(vertices, triangles);
        model->endModel();
        *geometry = model;
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    } catch (...) {
        return STATUS_INTERNAL_ERROR;
    }

    return STATUS_SUCCESS;
}

NTSTATUS BuildMeshBinding(
    const FCL_GEOMETRY_SNAPSHOT& snapshot,
    GeometryBinding* binding) noexcept {
    if (binding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = BuildMeshGeometry(snapshot, &binding->Geometry);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    binding->LocalTransform = fclmusa::geom::IdentityTransform();
    return STATUS_SUCCESS;
}

}  // namespace

FCL_TRANSFORM
CombineTransforms(
    const FCL_TRANSFORM& parent,
    const FCL_TRANSFORM& child) noexcept {
    FCL_TRANSFORM combined = {};
    combined.Rotation = fclmusa::geom::MultiplyMatrix(parent.Rotation, child.Rotation);
    const FCL_VECTOR3 rotatedChild = fclmusa::geom::MatrixVectorMultiply(parent.Rotation, child.Translation);
    combined.Translation = fclmusa::geom::Add(parent.Translation, rotatedChild);
    return combined;
}

fcl::Transform3d
ToEigenTransform(
    const FCL_TRANSFORM& transform) noexcept {
    fcl::Transform3d eigen = fcl::Transform3d::Identity();
    eigen.linear() = ToEigenMatrix(transform.Rotation);
    eigen.translation() = ToEigenVector(transform.Translation);
    return eigen;
}

NTSTATUS
BuildGeometryBinding(
    const FCL_GEOMETRY_SNAPSHOT& snapshot,
    GeometryBinding* binding) noexcept {
    if (binding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (snapshot.Type) {
        case FCL_GEOMETRY_SPHERE:
            return BuildSphereBinding(snapshot.Data.Sphere, binding);
        case FCL_GEOMETRY_OBB:
            return BuildObbBinding(snapshot.Data.Obb, binding);
        case FCL_GEOMETRY_MESH:
            return BuildMeshBinding(snapshot, binding);
        default:
            return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS
BuildCollisionObjects(
    const FCL_GEOMETRY_SNAPSHOT& object1,
    const FCL_TRANSFORM& transform1,
    const FCL_GEOMETRY_SNAPSHOT& object2,
    const FCL_TRANSFORM& transform2,
    CollisionObjects* objects,
    fcl::Transform3d& tf1,
    fcl::Transform3d& tf2) noexcept {
    if (objects == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = BuildGeometryBinding(object1, &objects->Object1);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = BuildGeometryBinding(object2, &objects->Object2);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    const FCL_TRANSFORM world1 = CombineTransforms(transform1, objects->Object1.LocalTransform);
    const FCL_TRANSFORM world2 = CombineTransforms(transform2, objects->Object2.LocalTransform);
    tf1 = ToEigenTransform(world1);
    tf2 = ToEigenTransform(world2);
    return STATUS_SUCCESS;
}

}  // namespace fclmusa::upstream
