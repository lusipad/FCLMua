#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fclmusa/geometry/bvh_model.h"

#include <algorithm>
#include <new>
#include <vector>

using namespace fclmusa::geom;

struct FCL_BVH_MODEL {
    const FCL_VECTOR3* Vertices = nullptr;
    const UINT32* Indices = nullptr;
    ULONG VertexCount = 0;
    ULONG IndexCount = 0;
    std::vector<FCL_BVH_NODE> Nodes;
    std::vector<UINT32> TriangleOrder;
};

namespace {

constexpr ULONG kLeafTriangleThreshold = 4;

struct TriangleInfo {
    FCL_OBBRSS Volume;
    FCL_VECTOR3 Centroid;
};

struct BuildContext {
    FCL_BVH_MODEL* Model = nullptr;
    ULONG TriangleCount = 0;
    std::vector<TriangleInfo> Infos;
    std::vector<UINT32> Order;
};

NTSTATUS BuildModelInternal(FCL_BVH_MODEL* model) noexcept;

FCL_OBBRSS ComputeTriangleVolume(
    const FCL_VECTOR3& a,
    const FCL_VECTOR3& b,
    const FCL_VECTOR3& c) noexcept {
    FCL_VECTOR3 points[3] = {a, b, c};
    return FclObbrssFromPoints(points, RTL_NUMBER_OF(points));
}

int ChooseSplitAxis(const BuildContext& ctx, ULONG begin, ULONG count) noexcept {
    FCL_VECTOR3 minPoint = ctx.Infos[ctx.Order[begin]].Centroid;
    FCL_VECTOR3 maxPoint = minPoint;
    for (ULONG i = 1; i < count; ++i) {
        const FCL_VECTOR3& centroid = ctx.Infos[ctx.Order[begin + i]].Centroid;
        minPoint.X = std::min(minPoint.X, centroid.X);
        minPoint.Y = std::min(minPoint.Y, centroid.Y);
        minPoint.Z = std::min(minPoint.Z, centroid.Z);
        maxPoint.X = std::max(maxPoint.X, centroid.X);
        maxPoint.Y = std::max(maxPoint.Y, centroid.Y);
        maxPoint.Z = std::max(maxPoint.Z, centroid.Z);
    }
    const FCL_VECTOR3 extent = Subtract(maxPoint, minPoint);
    if (extent.Y > extent.X && extent.Y >= extent.Z) {
        return 1;
    }
    if (extent.Z > extent.X && extent.Z >= extent.Y) {
        return 2;
    }
    return 0;
}

ULONG BuildRecursive(BuildContext& ctx, ULONG begin, ULONG count) {
    ULONG nodeIndex = static_cast<ULONG>(ctx.Model->Nodes.size());
    ctx.Model->Nodes.push_back({});
    FCL_BVH_NODE& node = ctx.Model->Nodes[nodeIndex];
    node.LeftChild = ULONG_MAX;
    node.RightChild = ULONG_MAX;
    node.FirstTriangle = begin;
    node.TriangleCount = count;

    FCL_OBBRSS combined = ctx.Infos[ctx.Order[begin]].Volume;
    for (ULONG i = 1; i < count; ++i) {
        combined = FclObbrssMerge(&combined, &ctx.Infos[ctx.Order[begin + i]].Volume);
    }
    node.Volume = combined;

    if (count <= kLeafTriangleThreshold) {
        return nodeIndex;
    }

    const int axis = ChooseSplitAxis(ctx, begin, count);
    const ULONG mid = begin + (count / 2);
    auto axisSelector = [axis](const FCL_VECTOR3& value) -> float {
        switch (axis) {
            case 1:
                return value.Y;
            case 2:
                return value.Z;
            default:
                return value.X;
        }
    };

    auto comparator = [&](UINT32 lhsIndex, UINT32 rhsIndex) {
        return axisSelector(ctx.Infos[lhsIndex].Centroid) <
               axisSelector(ctx.Infos[rhsIndex].Centroid);
    };

    std::nth_element(
        ctx.Order.begin() + begin,
        ctx.Order.begin() + mid,
        ctx.Order.begin() + begin + count,
        comparator);

    const ULONG leftCount = mid - begin;
    const ULONG rightCount = count - leftCount;

    node.LeftChild = BuildRecursive(ctx, begin, leftCount);
    node.RightChild = BuildRecursive(ctx, mid, rightCount);
    node.FirstTriangle = 0;
    node.TriangleCount = 0;
    return nodeIndex;
}

bool ValidateIndices(
    const FCL_VECTOR3* vertices,
    ULONG vertexCount,
    const UINT32* indices,
    ULONG indexCount) noexcept {
    if (indices == nullptr || vertices == nullptr) {
        return false;
    }
    if (indexCount % 3 != 0) {
        return false;
    }
    for (ULONG i = 0; i < indexCount; ++i) {
        if (indices[i] >= vertexCount) {
            return false;
        }
    }
    return true;
}

NTSTATUS BuildModelInternal(FCL_BVH_MODEL* model) noexcept {
    if (model == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    model->Nodes.clear();
    model->TriangleOrder.clear();

    BuildContext context = {};
    context.Model = model;
    context.TriangleCount = model->IndexCount / 3;

    try {
        context.Infos.resize(context.TriangleCount);
        context.Order.resize(context.TriangleCount);
        model->Nodes.reserve(context.TriangleCount * 2);
    } catch (const std::bad_alloc&) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (ULONG tri = 0; tri < context.TriangleCount; ++tri) {
        const UINT32 i0 = model->Indices[tri * 3];
        const UINT32 i1 = model->Indices[tri * 3 + 1];
        const UINT32 i2 = model->Indices[tri * 3 + 2];

        const FCL_VECTOR3& v0 = model->Vertices[i0];
        const FCL_VECTOR3& v1 = model->Vertices[i1];
        const FCL_VECTOR3& v2 = model->Vertices[i2];

        TriangleInfo info = {};
        info.Volume = ComputeTriangleVolume(v0, v1, v2);
        info.Centroid = Scale(Add(Add(v0, v1), v2), 1.0f / 3.0f);

        context.Infos[tri] = info;
        context.Order[tri] = tri;
    }

    if (context.TriangleCount > 0) {
        BuildRecursive(context, 0, context.TriangleCount);
    }

    model->TriangleOrder = std::move(context.Order);
    return STATUS_SUCCESS;
}

}  // namespace

extern "C"
NTSTATUS
FclBuildBvhModel(
    _In_reads_(vertexCount) const FCL_VECTOR3* vertices,
    _In_ ULONG vertexCount,
    _In_reads_(indexCount) const UINT32* indices,
    _In_ ULONG indexCount,
    _Outptr_ FCL_BVH_MODEL** model) noexcept {
    if (model == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *model = nullptr;

    if (vertices == nullptr || indices == nullptr || vertexCount == 0 || indexCount < 3) {
        return STATUS_INVALID_PARAMETER;
    }
    if (!ValidateIndices(vertices, vertexCount, indices, indexCount)) {
        return STATUS_INVALID_PARAMETER;
    }

    auto* instance = new (std::nothrow) FCL_BVH_MODEL();
    if (instance == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    instance->Vertices = vertices;
    instance->Indices = indices;
    instance->VertexCount = vertexCount;
    instance->IndexCount = indexCount;

    NTSTATUS status = BuildModelInternal(instance);
    if (!NT_SUCCESS(status)) {
        delete instance;
        return status;
    }

    *model = instance;
    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
FclBvhUpdateModel(
    _Inout_ FCL_BVH_MODEL* model,
    _In_reads_(vertexCount) const FCL_VECTOR3* vertices,
    _In_ ULONG vertexCount,
    _In_reads_(indexCount) const UINT32* indices,
    _In_ ULONG indexCount) noexcept {
    if (model == nullptr || vertices == nullptr || indices == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    if (vertexCount == 0 || indexCount < 3 || (indexCount % 3) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!ValidateIndices(vertices, vertexCount, indices, indexCount)) {
        return STATUS_INVALID_PARAMETER;
    }

    model->Vertices = vertices;
    model->VertexCount = vertexCount;
    model->Indices = indices;
    model->IndexCount = indexCount;

    return BuildModelInternal(model);
}

extern "C"
VOID
FclDestroyBvhModel(
    _In_opt_ FCL_BVH_MODEL* model) noexcept {
    delete model;
}

extern "C"
const FCL_BVH_NODE*
FclBvhGetNodes(
    _In_opt_ const FCL_BVH_MODEL* model,
    _Out_opt_ ULONG* nodeCount) noexcept {
    if (nodeCount != nullptr) {
        *nodeCount = (model != nullptr)
            ? static_cast<ULONG>(model->Nodes.size())
            : 0;
    }
    return (model != nullptr && !model->Nodes.empty()) ? model->Nodes.data() : nullptr;
}

extern "C"
const UINT32*
FclBvhGetTriangleOrder(
    _In_opt_ const FCL_BVH_MODEL* model,
    _Out_opt_ ULONG* triangleCount) noexcept {
    if (triangleCount != nullptr) {
        *triangleCount = (model != nullptr)
            ? static_cast<ULONG>(model->TriangleOrder.size())
            : 0;
    }
    return (model != nullptr && !model->TriangleOrder.empty()) ? model->TriangleOrder.data() : nullptr;
}
