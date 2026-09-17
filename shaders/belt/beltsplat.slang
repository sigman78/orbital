#include "../scene_shared.h"
#include "../scene/frame_records.slang"
#include "belt_frame.slang"

// Belt transmittance map, splat pass: every rock adds its cross-section to
// the light-space texel column it falls in, into one of four depth slices
// measured along the sun ray from the column's crossing of the belt plane.
// Additive blending accumulates coverage; a blur pass then softens it into
// the penumbra the sun's disc would give. No depth test, so nothing
// shimmers as rocks tumble.

[[vk::push_constant]] ConstantBuffer<CullRoot> root;

static const float SLICES = 4.0;

struct SplatVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 disk : DISK;                          // -1..1 across the splat
    [[vk::location(1)]] nointerpolation float2 weightSlice : WEIGHT; // coverage per texel, slice coordinate
};

[shader("vertex")]
SplatVaryings vertexMain(uint vertexIndex : SV_VertexID, uint instanceId : SV_InstanceID)
{
    const float2 corners[6] = { float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, -1), float2(1, 1), float2(-1, 1) };
    SplatVaryings output;
    CullParams* p = &root.scratch->params;
    output.disk = corners[vertexIndex];
    output.weightSlice = float2(0.0, 0.0);
    if (instanceId >= root.pass) // pass carries the caster count for this draw
    {
        output.position = float4(0.0, 0.0, -2.0, 1.0); // clipped away
        return output;
    }
    RockData rock = root.rocks[instanceId];
    float3 position = p->giant.xyz + rockPosition(root.state[uint(rock.rotation_seed.w + .5)]); // the tail's records carry their ids
    float radius = rock.position_radius.w;

    // Slice along the sun ray, relative to where this column crosses the belt plane.
    BeltParams belt = beltParams(root.frame);
    float3 light = normalize(lightParams(root.frame).position - position);
    float along = dot(position - p->giant.xyz, belt.normal) / (dot(light, belt.normal) + 1e-6);
    output.weightSlice.y = clamp((along / belt.lightSlice * .5 + .5) * SLICES, 0.0, SLICES - .001);

    // Splat at least one texel wide; the weight keeps the covered area right.
    float4 clip = mul(beltLightProjection(root.frame), float4(position, 1.0));
    float2 halfNdc = radius / belt.lightHalf;
    float2 texelNdc = 1.0 / float2(belt.lightTexels, belt.lightTexels); // half a texel each way
    float2 extent = max(halfNdc, texelNdc);
    output.weightSlice.x = min(1.0, (halfNdc.x * halfNdc.y) / (extent.x * extent.y));
    output.position = float4(clip.xy + output.disk * extent, clip.z, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(SplatVaryings input) : SV_Target0
{
    if (dot(input.disk, input.disk) > 1.0) discard;
    int slice = int(input.weightSlice.y);
    float4 opacity = float4(0.0, 0.0, 0.0, 0.0);
    opacity[slice] = input.weightSlice.x;
    return opacity;
}
