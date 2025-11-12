#include "fclmusa/upstream_bridge.h"

#include <memory>
#include <vector>

#include <fcl/geometry/bvh/BVH_model.h>
#include <fcl/geometry/shape/box.h>
#include <fcl/geometry/shape/sphere.h>
#include <fcl/narrowphase/collision.h>
#include <fcl/narrowphase/continuous_collision.h>
#include <fcl/narrowphase/distance.h>

#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/logging.h"

namespace {

using GeometryPtr = std::shared_ptr<fcl::CollisionGeometryd>;
using BVHModeld = fcl::BVHModel<fcl::OBBRSS<double>>;

FCL_TRANSFORM CombineTransforms(const FCL_TRANSFORM& parent, const FCL_TRANSFORM& child) noexcept {
    FCL_TRANSFORM combined = {};
    combined.Rotation = fclmusa::geom::MultiplyMatrix(parent.Rotation, child.Rotation);
    const FCL_VECTOR3 rotatedChild = fclmusa::geom::MatrixVectorMultiply(parent.Rotation, child.Translation);
    combined.Translation = fclmusa::geom::Add(parent.Translation, rotatedChild);
    return combined;
}

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

FCL_VECTOR3 ToVector3(const fcl::Vector3d& vector) noexcept {
    return {
        static_cast<float>(vector.x()),
        static_cast<float>(vector.y()),
        static_cast<float>(vector.z())};
}

fcl::Transform3d ToEigenTransform(const FCL_TRANSFORM& transform) noexcept {
    fcl::Transform3d eigen = fcl::Transform3d::Identity();
    eigen.linear() = ToEigenMatrix(transform.Rotation);
    eigen.translation() = ToEigenVector(transform.Translation);
    return eigen;
}

struct GeometryBinding {
    GeometryPtr Geometry;
    FCL_TRANSFORM LocalTransform = fclmusa::geom::IdentityTransform();
};

NTSTATUS BuildSphereBinding(
    const FCL_SPHERE_GEOMETRY_DESC& desc,
    GeometryBinding* binding) noexcept {
    if (desc.Radius <= 0.0f || binding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    try {
        binding->Geometry = std::make_shared<fcl::Sphered>(static_cast<double>(desc.Radius));
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
        binding->Geometry = std::make_shared<fcl::Boxd>(
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
        auto model = std::make_shared<BVHModeld>();
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

NTSTATUS BuildGeometryBinding(
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

struct CollisionObjects {
    GeometryBinding Object1;
    GeometryBinding Object2;
};

NTSTATUS BuildCollisionObjects(
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

void WriteContact(
    const fcl::CollisionResultd& upstream,
    _Out_ PFCL_CONTACT_INFO contactInfo) noexcept {
    if (contactInfo == nullptr) {
        return;
    }

    RtlZeroMemory(contactInfo, sizeof(*contactInfo));
    if (upstream.isCollision() && upstream.numContacts() > 0) {
        const fcl::Contactd& contact = upstream.getContact(0);
        contactInfo->Normal = ToVector3(contact.normal);
        contactInfo->PenetrationDepth = static_cast<float>(contact.penetration_depth);
        contactInfo->PointOnObject1 = ToVector3(contact.pos);
        const FCL_VECTOR3 pen = fclmusa::geom::Scale(contactInfo->Normal, contactInfo->PenetrationDepth);
        contactInfo->PointOnObject2 = fclmusa::geom::Add(contactInfo->PointOnObject1, pen);
    }
}

void WriteDistance(
    const fcl::DistanceResultd& upstream,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept {
    if (result == nullptr) {
        return;
    }
    result->Distance = static_cast<float>(upstream.min_distance);
    result->ClosestPoint1 = ToVector3(upstream.nearest_points[0]);
    result->ClosestPoint2 = ToVector3(upstream.nearest_points[1]);
}

void WriteContinuousContact(
    const fcl::ContinuousCollisionResultd& upstream,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept {
    if (result == nullptr) {
        return;
    }

    result->Intersecting = upstream.is_collide ? TRUE : FALSE;
    result->TimeOfImpact = upstream.time_of_contact;
    if (upstream.is_collide) {
        result->Contact.PointOnObject1 = ToVector3(upstream.contact_tf1.translation());
        result->Contact.PointOnObject2 = ToVector3(upstream.contact_tf2.translation());
        result->Contact.Normal = {0.0f, 0.0f, 0.0f};
        result->Contact.PenetrationDepth = 0.0f;
    } else {
        RtlZeroMemory(&result->Contact, sizeof(result->Contact));
    }
}

NTSTATUS HandleException(const std::exception& ex) noexcept {
    FCL_LOG_ERROR("Upstream FCL threw exception: %s", ex.what());
    return STATUS_INTERNAL_ERROR;
}

}  // namespace

NTSTATUS
FclUpstreamCollide(
    _In_ const FCL_GEOMETRY_SNAPSHOT& object1,
    _In_ const FCL_TRANSFORM& transform1,
    _In_ const FCL_GEOMETRY_SNAPSHOT& object2,
    _In_ const FCL_TRANSFORM& transform2,
    _Out_ PBOOLEAN isColliding,
    _Out_opt_ PFCL_CONTACT_INFO contactInfo) noexcept {
    if (isColliding == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    try {
        CollisionObjects objects = {};
        fcl::Transform3d tf1 = fcl::Transform3d::Identity();
        fcl::Transform3d tf2 = fcl::Transform3d::Identity();
        NTSTATUS status = BuildCollisionObjects(object1, transform1, object2, transform2, &objects, tf1, tf2);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        fcl::CollisionObjectd collisionObject1(objects.Object1.Geometry, tf1);
        fcl::CollisionObjectd collisionObject2(objects.Object2.Geometry, tf2);

        fcl::CollisionRequestd request;
        if (contactInfo != nullptr) {
            request.enable_contact = true;
            request.num_max_contacts = 1;
        }

        fcl::CollisionResultd result;
        fcl::collide(&collisionObject1, &collisionObject2, request, result);
        *isColliding = result.isCollision() ? TRUE : FALSE;
        if (contactInfo != nullptr) {
            WriteContact(result, contactInfo);
        }
        return STATUS_SUCCESS;
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    } catch (const std::exception& ex) {
        return HandleException(ex);
    } catch (...) {
        return STATUS_INTERNAL_ERROR;
    }
}

NTSTATUS
FclUpstreamDistance(
    _In_ const FCL_GEOMETRY_SNAPSHOT& object1,
    _In_ const FCL_TRANSFORM& transform1,
    _In_ const FCL_GEOMETRY_SNAPSHOT& object2,
    _In_ const FCL_TRANSFORM& transform2,
    _Out_ PFCL_DISTANCE_RESULT result) noexcept {
    if (result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    try {
        CollisionObjects objects = {};
        fcl::Transform3d tf1 = fcl::Transform3d::Identity();
        fcl::Transform3d tf2 = fcl::Transform3d::Identity();
        NTSTATUS status = BuildCollisionObjects(object1, transform1, object2, transform2, &objects, tf1, tf2);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        fcl::CollisionObjectd collisionObject1(objects.Object1.Geometry, tf1);
        fcl::CollisionObjectd collisionObject2(objects.Object2.Geometry, tf2);

        fcl::DistanceRequestd request(true);
        request.enable_nearest_points = true;
        request.enable_signed_distance = false;
        fcl::DistanceResultd distanceResult;
        fcl::distance(&collisionObject1, &collisionObject2, request, distanceResult);
        WriteDistance(distanceResult, result);
        return STATUS_SUCCESS;
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    } catch (const std::exception& ex) {
        return HandleException(ex);
    } catch (...) {
        return STATUS_INTERNAL_ERROR;
    }
}

NTSTATUS
FclUpstreamContinuousCollision(
    _In_ const FCL_GEOMETRY_SNAPSHOT& object1,
    _In_ const FCL_INTERP_MOTION& motion1,
    _In_ const FCL_GEOMETRY_SNAPSHOT& object2,
    _In_ const FCL_INTERP_MOTION& motion2,
    double tolerance,
    ULONG maxIterations,
    _Out_ PFCL_CONTINUOUS_COLLISION_RESULT result) noexcept {
    if (result == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    const double resolvedTolerance = (tolerance > 0.0) ? tolerance : 1.0e-4;
    const std::size_t resolvedIterations = (maxIterations > 0) ? maxIterations : 64;

    try {
        GeometryBinding binding1 = {};
        GeometryBinding binding2 = {};
        NTSTATUS status = BuildGeometryBinding(object1, &binding1);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        status = BuildGeometryBinding(object2, &binding2);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        const FCL_TRANSFORM start1 = CombineTransforms(motion1.Start, binding1.LocalTransform);
        const FCL_TRANSFORM end1 = CombineTransforms(motion1.End, binding1.LocalTransform);
        const FCL_TRANSFORM start2 = CombineTransforms(motion2.Start, binding2.LocalTransform);
        const FCL_TRANSFORM end2 = CombineTransforms(motion2.End, binding2.LocalTransform);

        fcl::Transform3d tfStart1 = ToEigenTransform(start1);
        fcl::Transform3d tfEnd1 = ToEigenTransform(end1);
        fcl::Transform3d tfStart2 = ToEigenTransform(start2);
        fcl::Transform3d tfEnd2 = ToEigenTransform(end2);

        fcl::ContinuousCollisionRequestd request(
            resolvedIterations,
            resolvedTolerance,
            fcl::CCDM_LINEAR,
            fcl::GJKSolverType::GST_LIBCCD,
            fcl::CCDC_CONSERVATIVE_ADVANCEMENT);

        fcl::ContinuousCollisionResultd upstream;
        fcl::continuousCollide(
            binding1.Geometry.get(),
            tfStart1,
            tfEnd1,
            binding2.Geometry.get(),
            tfStart2,
            tfEnd2,
            request,
            upstream);

        WriteContinuousContact(upstream, result);
        return STATUS_SUCCESS;
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    } catch (const std::exception& ex) {
        return HandleException(ex);
    } catch (...) {
        return STATUS_INTERNAL_ERROR;
    }
}
