#include "../scene_shared.h"

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
    uint packed = uint(rock.spin_group.w + .5);
    uint band = packed & (ORBITAL_BELT_BANDS - 1);
    float2 spin = p->band_spin[band].xy;
    float3 v = rock.position_radius.xyz;
    float bx = v.x * spin.x - v.z * spin.y, bz = v.x * spin.y + v.z * spin.x;
    float3 local = float3(bx, v.y * p->belt_tilt.x - bz * p->belt_tilt.y, bz * p->belt_tilt.z);
    float3 position = p->giant.xyz + local;
    float radius = rock.position_radius.w;

    // Slice along the sun ray, relative to where this column crosses the belt plane.
    float3 n = root.frame->belt_normal.xyz;
    float3 light = normalize(root.frame->sun.xyz - position);
    float along = dot(position - p->giant.xyz, n) / (dot(light, n) + 1e-6);
    float halfSpan = root.frame->belt_light.w;
    output.weightSlice.y = clamp((along / halfSpan * .5 + .5) * SLICES, 0.0, SLICES - .001);

    // Splat at least one texel wide; the weight keeps the covered area right.
    float4 clip = mul(root.frame->belt_light_projection, float4(position, 1.0));
    float2 halfNdc = radius / root.frame->belt_light.xy;
    float2 texelNdc = 1.0 / root.frame->belt_light.zz; // half a texel each way
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
