#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "types.slang"
#include "../scene/instances.slang"
#include "../planets/cloud_geometry.slang"
#include "../lib/math.slang"
#include "patch.slang"

// The vertex stage every surface pipeline shares: bodies, the pooled rock
// draws, the Earth's cloud shell, the shadow pass and the rock billboards.

[shader("vertex")]
SurfaceVaryings vertexMain(uint vertexIndex : SV_VertexID, uint firstVertex : SV_StartVertexLocation,
                           uint instanceId : SV_InstanceID, uint firstInstance : SV_StartInstanceLocation)
{
    // SV_VertexID and SV_InstanceID are zero based per draw; the pooled rock
    // draws place their group's slice through the vertex-offset and
    // first-instance arguments.
    vertexIndex += firstVertex;
    SurfaceVaryings output;
    output.instanceIndex = root.base + firstInstance + instanceId;
    Instance inst = loadInstance(root.frame, root.instances, output.instanceIndex,
                                 root.mode == MODE_BILLBOARD || root.mode == MODE_SPLAT_MASK);
    ViewParams view = viewParams(root.frame);
    if (root.mode == MODE_BILLBOARD || root.mode == MODE_SPLAT_MASK)
    {
        const float2 corners[6] = {float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
        float2 q = corners[vertexIndex];
        output.worldPosition = inst.center_radius.xyz + (view.right*q.x + view.up*q.y)*inst.center_radius.w;
        output.worldNormal = -view.forward; output.localNormal = float3(q,0);
        output.position = mul(viewProjection(root.frame), float4(output.worldPosition,1));
        return output;
    }
    if (root.flags & ORBITAL_ROOT_PATCHES)
    {
        PatchResult pr = evaluatePatch(vertexIndex, instanceId, root);
        output.worldPosition = pr.worldPosition;
        output.worldNormal = pr.worldNormal;
        output.localNormal = pr.localNormal;
        output.patchLevel = pr.level;
        output.position = mul(root.mode == MODE_SHADOW ? shadowProjection(root.frame) : viewProjection(root.frame), float4(output.worldPosition, 1.0));
        return output;
    }
    output.patchLevel = 0;
    Vertex vertex = root.vertices[vertexIndex];
    float3x3 rotate = rotation(inst.rotation_kind.xyz);
    float scale = inst.center_radius.w * (root.mode == MODE_CLOUD ? 1.0 + CLOUD_HEIGHT : 1.0);
    output.worldPosition = inst.center_radius.xyz + mul(rotate, vertex.position.xyz) * scale;
    output.worldNormal = normalize(mul(rotate, vertex.normal.xyz)); output.localNormal = normalize(vertex.position.xyz);
    output.position = mul(root.mode == MODE_SHADOW ? shadowProjection(root.frame) : viewProjection(root.frame), float4(output.worldPosition, 1.0));
    return output;
}
