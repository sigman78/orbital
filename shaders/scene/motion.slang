#include "frame_bindings.slang"
#include "frame_records.slang"
#include "../surface/types.slang"
#include "instances.slang"
#include "../lib/math.slang"
#include "view.slang"

// Motion vectors for the opaque meshes, the rocks and the bodies: a pass after
// the scene pass at equal depth, so only the visible surface writes. Each vertex
// is placed twice, where it is and where it was a frame ago (the instance's
// previous centre and rotation, the previous camera through the temporal
// record), and the fragment stores the screen-space step from here to there in
// the jittered raster's UV units. A triangle at the far plane then fills what
// nothing opaque covered with the sky's step, the camera's turn alone, so the
// field is complete and the temporal pass can sample it between pixels.

struct MotionVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float4 previousClip : PREVIOUS_CLIP;
};

[shader("vertex")]
MotionVaryings vertexMain(uint vertexIndex : SV_VertexID, uint firstVertex : SV_StartVertexLocation,
                          uint instanceId : SV_InstanceID, uint firstInstance : SV_StartInstanceLocation)
{
    if (root.mode == ORBITAL_SURFACE_MOTION_SKY)
    {
        MotionVaryings sky;
        float2 corner = float2((vertexIndex << 1) & 2, vertexIndex & 2);
        sky.position = float4(corner * 2.0 - 1.0, 1.0, 1.0); // the far plane: only where nothing was drawn
        sky.previousClip = float4(0.0, 0.0, 0.0, 0.0);
        return sky;
    }
    vertexIndex += firstVertex;
    Instance inst = loadInstance(root.frame, root.instances, root.base + firstInstance + instanceId, false);
    Vertex vertex = root.vertices[vertexIndex];
    float scale = inst.center_radius.w;
    float3 world = inst.center_radius.xyz + mul(rotation(inst.rotation_kind.xyz), vertex.position.xyz) * scale;
    // Relative to the previous camera: the current centre, the instance's own step back
    // and the camera's, then the previous rotation.
    float3 previousCentre = inst.center_radius.xyz + inst.previous_center.xyz + temporalParams(root.frame).cameraDelta;
    float3 previous = previousCentre
                    + mul(rotation(inst.rotation_kind.xyz + inst.previous_rotation.xyz), vertex.position.xyz) * scale;
    MotionVaryings output;
    output.position = mul(viewProjection(root.frame), float4(world, 1.0));
    output.previousClip = mul(previousProjection(root.frame), float4(previous, 1.0));
    return output;
}

[shader("fragment")]
float2 fragmentMain(MotionVaryings input) : SV_Target0
{
    ViewParams view = viewParams(root.frame);
    float4 previousClip = input.previousClip;
    if (root.mode == ORBITAL_SURFACE_MOTION_SKY)
        previousClip = mul(previousProjection(root.frame), float4(rayDirection(root.frame, input.position.xy), 0.0));
    if (previousClip.w <= 1e-5) return float2(1e4, 1e4); // behind the previous camera: lands outside the history
    float2 previousUV = previousClip.xy / previousClip.w * .5 + .5 - view.previousJitter / view.size;
    return previousUV - input.position.xy / view.size;
}
