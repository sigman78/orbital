#include "../scene_shared.h"

// GPU belt culling: one thread per rock decides visibility, level and
// placement, three passes turn that into per-group instance lists and
// indirect draw arguments. Everything is addressed through buffer pointers;
// the conventional descriptor set carries no storage buffers.
//
//   ORBITAL_CULL_PASS_COUNT    each visible rock bumps its group's counter
//   ORBITAL_CULL_PASS_PREFIX   one thread turns counts into bases and draw arguments
//   ORBITAL_CULL_PASS_SCATTER  each visible rock claims a slot and writes its instance

#include "../scene/bindings.slang"
#include "../scene/frame_records.slang"
[[vk::push_constant]] ConstantBuffer<CullRoot> root;

#include "belt.slang"

// Two splat regimes: a soft-edged disc of the rock's true size once it spans
// half a pixel or more, and below that a one-pixel Gaussian point whose peak
// alpha stays under one, so a tiny rock never lights a whole pixel the way a
// mesh only sometimes would.
static const float SPLAT_DISC_PIXELS = .5;
static const float SPLAT_POINT_PIXELS = 1.0;
static const float SPLAT_AMBIENT = .004;
static const float SPLAT_RIM = .12;       // forward-scatter rim the meshes' grazing specular gives at high phase angles

static const uint BILLBOARD_GROUP = ORBITAL_ROCK_GROUPS;
static const float BILLBOARD_VERTICES = 6.0;

struct Placement {
    bool visible;
    uint group;
    AsteroidInstance instance;
};

// Composition classes, chosen by radius and chance like the main belt's
// taxonomic gradient: bright reddish silicate (S) rocks dominate inside,
// dark neutral carbonaceous (C) rocks outside, with a few grey metallic (M).
// The class rides in the integer part of Instance.tint.w, the material seed
// in its fraction; billboards carry their coverage there instead.
#include "rockclass.slang"
#include "belt_frame.slang"

// True when the rock's sphere lies entirely behind one of the bodies, seen from
// the cull camera; both are live camera relative, so the frozen offset shifts both.
bool occludedByBodies(float3 target, float targetRadius, float3 offset)
{
    target += offset;
    float targetDistance = length(target);
    if (targetDistance <= targetRadius) return false;
    float3 direction = target / targetDistance;
    float targetAngle = asin(saturate(targetRadius / targetDistance));
    uint bodyCount = root.scratch->params.body_count;
    for (uint i = 0; i < 8; ++i)
    {
        if (i >= bodyCount) break;
        BodyGpu body = bodyAt(root.frame, int(i));
        float3 centre = body.centre + offset;
        float bodyDistance = length(centre);
        if (bodyDistance <= body.radius || targetDistance - targetRadius <= bodyDistance + body.radius) continue;
        float separation = acos(clamp(dot(direction, centre / bodyDistance), -1.0, 1.0));
        float bodyAngle = asin(saturate(body.radius / bodyDistance));
        if (separation + targetAngle < bodyAngle) return true;
    }
    return false;
}

Placement placeRock(uint id, bool lightSplats)
{
    Placement result;
    result.visible = false;
    result.group = 0;
    RockData rock = root.rocks[id];
    CullParams* p = &root.scratch->params;
    uint variant = uint(rock.spin_group.w + .5) / ORBITAL_BELT_BANDS;
    // The CPU sweep places the rock in the belt plane each frame; the record keeps what does not move.
    float4 state = root.state[id];
    float3 position = p->giant.xyz + rockPosition(state);
    result.instance.previous[0] = 0; result.instance.previous[1] = 0;
    result.instance.previous[2] = 0; result.instance.previous[3] = 0;
    float radius = rock.position_radius.w;
    float3 v = rock.position_radius.xyz; // the belt frame before the spin, for the radial class
    // Visibility, level and coverage are judged from the cull camera, which the
    // panel can freeze while the live camera moves on; the instance keeps the
    // live camera relative position, so what was culled draws where it is.
    float3 eye = position + p->freeze.xyz;

    float z = dot(eye, p->forward.xyz);
    if (z < -radius) return result;
    float pixelRadius = radius * p->view.x / max(z, .1);
    if (pixelRadius < p->billboard.z) return result;
    // Frustum planes with non-unit normals; padding covers jitter and expanded billboards.
    float zc = max(z, 0.0);
    float padding = radius + zc * p->forward.w;
    if (abs(dot(eye, p->right.xyz)) > zc * p->right.w + padding * p->view.y) return result;
    if (abs(dot(eye, p->up.xyz)) > zc * p->up.w + padding * p->view.z) return result;
    if (occludedByBodies(position, radius, p->freeze.xyz)) return result;

    result.visible = true;
    BeltParams belt = beltParams(root.frame);
    float radial = (length(v.xz) - belt.inner) / (belt.outer - belt.inner);
    uint composition = rockClass(id, radial);
    float3 colour = rockColour(composition, id);
    if (pixelRadius < p->billboard.y)
    {
        bool onDisc = radius <= DISC_BAKE_MAX_RADIUS; // small rocks live in the far-belt map once the camera is out
        if (onDisc && p->billboard.w >= .99) { result.visible = false; return result; }
        result.group = BILLBOARD_GROUP;
        if (!lightSplats) return result; // the count pass only needs the group
        // Sub-pixel rocks become splats lit here, once per rock: a Lambert sphere
        // at its phase angle, shadowed by bodies, belt dust and the belt map. The
        // splat is a pixel-filter footprint whose coverage matches the rock's
        // true covered area, so small rocks stay small and dim.
        Frame* frame = root.frame;
        LightParams sunLight = lightParams(frame);
        bool disc = pixelRadius >= SPLAT_DISC_PIXELS;
        float splatPixels = disc ? pixelRadius * sqrt(SPLAT_AREA) : SPLAT_POINT_PIXELS;
        float coverage = disc ? 1.0 : SPLAT_AREA * pixelRadius * pixelRadius / (SPLAT_POINT_PIXELS * SPLAT_POINT_PIXELS);
        coverage *= saturate((pixelRadius - p->billboard.z) / p->billboard.z);
        if (onDisc) coverage *= 1.0 - p->billboard.w; // the far-belt disc carries the rest
        float3 light = normalize(sunLight.position - position);
        float phase = acos(clamp(dot(light, normalize(-position)), -1.0, 1.0));
        // The same scanned set the mesh material would lead with, at its coarsest mip.
        float seed = rockMaterialSeed(id);
        float3 average = composition == CLASS_C ? textures[TEX_ROCK_FACE_ALBEDO].SampleLevel(samplers[SAMPLER_WRAP_U_ANISOTROPIC], float2(.5, .5), 10.0).rgb
                       : seed < .5 ? textures[TEX_ROCK_ALBEDO].SampleLevel(samplers[SAMPLER_WRAP_U_ANISOTROPIC], float2(.5, .5), 10.0).rgb
                                   : textures[TEX_ROCK_BOULDER_ALBEDO].SampleLevel(samplers[SAMPLER_WRAP_U_ANISOTROPIC], float2(.5, .5), 10.0).rgb;
        float3 albedo = average * colour * (.8 + .4 * frac(seed * 5.1));
        // Everything but the cosine term is per rock. Discs are shaded as sphere
        // impostors in the fragment (cosine per pixel), points take the disc average.
        float rim = SPLAT_RIM * (1.0 - cos(phase)) * .5;
        float shadowing = solarVisibility(frame, position, -1) * beltExtinction(frame, position) * beltRockTransmittance(frame, position);
        float irradiance = sunLight.radiance / 3.141592653589793 * shadowing;
        result.group = BILLBOARD_GROUP;
        result.instance.center_radius = float4(position, radius * splatPixels / pixelRadius);
        float3 radiance = albedo * irradiance * (disc ? 1.0 : lambertSpherePhase(phase) + rim + SPLAT_AMBIENT / max(irradiance, 1e-6));
        // RGB is bounded by the fixed sun irradiance and normalized belt materials.
        // Keep ambient (large in deep shadow) and coverage in float32.
        result.instance.payload[0] = packHalf2x16(radiance.xy);
        result.instance.payload[1] = packHalf2x16(float2(radiance.z, rim)) | (disc ? 0x80000000u : 0u);
        result.instance.payload[2] = asuint(SPLAT_AMBIENT / max(irradiance, 1e-6));
        result.instance.payload[3] = asuint(coverage);
        return result;
    }
    uint level = 0;
    if (pixelRadius >= p->levels.x) level = 1;
    if (pixelRadius >= p->levels.y) level = 2;
    if (pixelRadius >= p->levels.z) level = 3;
    if (pixelRadius >= p->levels.w) level = 4;
    if (pixelRadius >= p->billboard.x) level = 5;
    result.group = variant * ORBITAL_ROCK_LEVELS + level;
    float3 rotation = rockRotation(state, rock);
    result.instance.center_radius = float4(position, radius);
    result.instance.payload[0] = asuint(rotation.x);
    result.instance.payload[1] = asuint(rotation.y);
    result.instance.payload[2] = asuint(rotation.z);
    result.instance.payload[3] = id | (composition << 30);
    // The step back to the previous frame for the motion pass: the belt parent's
    // world step plus the rock's own, and the tumble's along its axis.
    float4 previous = root.previous_state[id];
    float3 step = p->giant_motion.xyz + rockPosition(previous) - rockPosition(state);
    float3 tumble = rock.spin_group.xyz * (previous.w - state.w);
    result.instance.previous[0] = packHalf2x16(step.xy);
    result.instance.previous[1] = packHalf2x16(float2(step.z, tumble.x));
    result.instance.previous[2] = packHalf2x16(tumble.yz);
    return result;
}

[shader("compute")]
[numthreads(ORBITAL_CULL_THREADS, 1, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    CullScratch* scratch = root.scratch;
    uint id = thread.x;
    if (root.pass == ORBITAL_CULL_PASS_PREFIX)
    {
        if (id != 0) return;
        uint running = scratch->params.body_count, drawCount = 0;
        for (uint g = 0; g <= ORBITAL_ROCK_GROUPS; ++g)
        {
            uint count = scratch->counts[g].load();
            scratch->cursors[g].store(running);
            DrawArgs args;
            args.pad0 = 0; args.pad1 = 0; args.pad2 = 0;
            if (g == BILLBOARD_GROUP)
            {
                // VkDrawIndirectCommand: vertex count, instance count, first vertex, first instance.
                args.index_count = uint(BILLBOARD_VERTICES);
                args.instance_count = count;
                args.first_index = 0;
                args.vertex_offset = running;
                args.first_instance = 0;
                scratch->args[BILLBOARD_GROUP] = args;
            }
            else if (count > 0)
            {
                // Empty groups are dropped, so the multi-draw only visits groups with rocks.
                args.index_count = scratch->params.index_counts[g];
                args.instance_count = count;
                args.first_index = scratch->params.first_indices[g];
                args.vertex_offset = scratch->params.vertex_offsets[g];
                args.first_instance = running;
                scratch->args[drawCount++] = args;
            }
            running += count;
        }
        scratch->draw_count = drawCount;
        return;
    }
    if (id >= scratch->params.rock_limit) return;
    Placement placement = placeRock(id, root.pass != ORBITAL_CULL_PASS_COUNT || scratch->params.light_in_count_pass != 0);
    if (!placement.visible) return;
    if (root.pass == ORBITAL_CULL_PASS_COUNT)
        scratch->counts[placement.group].add(1);
    else
    {
        uint slot = scratch->cursors[placement.group].add(1);
        scratch->instances[slot - scratch->params.body_count] = placement.instance;
    }
}
