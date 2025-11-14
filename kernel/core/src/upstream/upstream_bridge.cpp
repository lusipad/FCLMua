#include "fclmusa/upstream/upstream_bridge.h"

#include <memory>
#include <vector>

#include <fcl/geometry/bvh/BVH_model.h>
#include <fcl/narrowphase/collision.h>
#include <fcl/narrowphase/continuous_collision.h>
#include <fcl/narrowphase/distance.h>

#include "fclmusa/upstream/geometry_bridge.h"
#include "fclmusa/geometry/math_utils.h"
#include "fclmusa/logging.h"

namespace {

using fclmusa::upstream::CollisionObjects;
using fclmusa::upstream::GeometryBinding;
using fclmusa::upstream::ToEigenTransform;
using fclmusa::upstream::CombineTransforms;
using fclmusa::upstream::BuildGeometryBinding;
using fclmusa::upstream::BuildCollisionObjects;

FCL_VECTOR3 ToVector3(const fcl::Vector3d& vector) noexcept {
    return {
        static_cast<float>(vector.x()),
        static_cast<float>(vector.y()),
        static_cast<float>(vector.z())};
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
