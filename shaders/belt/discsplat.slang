#include "../scene_shared.h"
#include "../scene/frame_records.slang"
#include "rockclass.slang"
#include "belt_frame.slang"

// Far-belt map, rock layer: every rock adds its projected cross-section,
// times its albedo, to the belt-plane texel it falls in (additive blend).
// The map holds coverage times albedo in rgb and coverage in alpha, per unit
// plane area; the far tier turns it into a volume density with the belt's
// vertical profile and lights it with the phase and the baked sunlight.
// Refreshed every few dozen frames, since the rocks only drift along their bands.

[[vk::push_constant]] ConstantBuffer<CullRoot> root;


struct SplatVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 disk : DISK;                     // -1..1 across the splat
    [[vk::location(1)]] nointerpolation float4 value : VALUE;   // coverage * albedo, coverage
};

[shader("vertex")]
SplatVaryings vertexMain(uint vertexIndex : SV_VertexID, uint instanceId : SV_InstanceID)
{
    const float2 corners[6] = { float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, -1), float2(1, 1), float2(-1, 1) };
    SplatVaryings output;
    CullParams* p = &root.scratch->params;
    output.disk = corners[vertexIndex];
    output.value = float4(0.0, 0.0, 0.0, 0.0);
    if (instanceId >= root.pass || root.rocks[instanceId].position_radius.w > DISC_BAKE_MAX_RADIUS) // pass carries the rock count for this draw
    {
        output.position = float4(0.0, 0.0, -2.0, 1.0); // clipped away
        return output;
    }
    RockData rock = root.rocks[instanceId];
    float3 position = beltParams(root.frame).centre + rockPosition(root.state[instanceId]); // this frame's centre, not the cull's
    float radius = rock.position_radius.w;
    float3 v = rock.position_radius.xyz; // the belt frame before the spin, for the radial class

    BeltParams belt = beltParams(root.frame);
    float radial = (length(v.xz) - belt.inner) / (belt.outer - belt.inner);
    uint composition = rockClass(instanceId, radial);
    float3 albedo = rockColour(composition, instanceId) * (composition == CLASS_C ? ROCK_SET_AVERAGE_C : ROCK_SET_AVERAGE_S);

    // Splat at least ORBITAL_ROCK_MAP_FOOTPRINT texels each way; the weight keeps the covered area
    // right. The map is a rock-count estimate and wants smoothing, and spreading each
    // rock here is the smoothing: the far march then reads one texel where it box-filtered
    // sixteen.
    BeltDiscParams disc = beltDiscParams(root.frame);
    float4 clip = mul(beltDiscProjection(root.frame), float4(position, 1.0));
    float halfExtent = disc.half.x, texels = disc.rockTexels;
    float2 halfNdc = radius / halfExtent;
    float2 texelNdc = ORBITAL_ROCK_MAP_FOOTPRINT / texels; // half the footprint each way, in texels
    float2 extent = max(halfNdc, texelNdc);
    float texelArea = (2.0 * halfExtent / texels) * (2.0 * halfExtent / texels);
    float coverage = SPLAT_AREA * 3.141592653589793 * radius * radius / texelArea; // area fraction of one texel
    coverage *= (halfNdc.x * halfNdc.y) / (extent.x * extent.y);           // spread over the enlarged footprint
    output.value = float4(albedo * coverage, coverage);
    output.position = float4(clip.xy + output.disk * extent, .5, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(SplatVaryings input) : SV_Target0
{
    if (dot(input.disk, input.disk) > 1.0) discard;
    return input.value;
}
