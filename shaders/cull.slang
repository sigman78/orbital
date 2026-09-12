#include "scene_shared.h"

// GPU belt culling: one thread per rock decides visibility, level and
// placement, three passes turn that into per-group instance lists and
// indirect draw arguments. Everything is addressed through buffer pointers;
// the conventional descriptor set carries no storage buffers.
//
//   pass 0  count    each visible rock bumps its group's counter
//   pass 1  prefix   one thread turns counts into bases and draw arguments
//   pass 2  scatter  each visible rock claims a slot and writes its instance

[[vk::push_constant]] ConstantBuffer<CullRoot> root;

static const uint BILLBOARD_GROUP = ORBITAL_ROCK_GROUPS;
static const float KIND_ROCK = 3.0; // SurfaceKind::rock, also SurfaceMode::billboard
static const float BILLBOARD_VERTICES = 6.0;

struct Placement {
    bool visible;
    uint group;
    Instance instance;
};

// Composition classes, chosen by radius and chance like the main belt's
// taxonomic gradient: bright reddish silicate (S) rocks dominate inside,
// dark neutral carbonaceous (C) rocks outside, with a few grey metallic (M).
// The class rides in the integer part of Instance.tint.w, the material seed
// in its fraction; billboards carry their coverage there instead.
static const uint CLASS_S = 0, CLASS_C = 1, CLASS_M = 2;
static const float METAL_FRACTION = .03;

float hashId(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return float(x & 0xffffffu) / 16777216.0; }
float rockMaterialSeed(uint id) { return hashId(id * 2654435761u + 17u); }

uint rockClass(uint id, float radial)
{
    float carbon = lerp(.12, .85, smoothstep(0.0, 1.0, radial));
    float h = hashId(id * 747796405u + 5u);
    if (h < METAL_FRACTION) return CLASS_M;
    return h < METAL_FRACTION + carbon * (1.0 - METAL_FRACTION) ? CLASS_C : CLASS_S;
}

float3 rockColour(uint rockClass, uint id)
{
    float3 base = rockClass == CLASS_S ? float3(.92, .78, .62) : rockClass == CLASS_C ? float3(.42, .40, .38) : float3(.74, .76, .80);
    float variation = .8 + .4 * hashId(id * 2891336453u + 3u);
    return base * variation;
}

// True when the rock's sphere lies entirely behind one of the bodies.
bool occludedByBodies(float3 target, float targetRadius)
{
    float targetDistance = length(target);
    if (targetDistance <= targetRadius) return false;
    float3 direction = target / targetDistance;
    float targetAngle = asin(saturate(targetRadius / targetDistance));
    uint bodyCount = root.scratch->params.body_count;
    for (uint i = 0; i < 8; ++i)
    {
        if (i >= bodyCount) break;
        float4 body = root.frame->bodies[i];
        float bodyDistance = length(body.xyz);
        if (bodyDistance <= body.w || targetDistance - targetRadius <= bodyDistance + body.w) continue;
        float separation = acos(clamp(dot(direction, body.xyz / bodyDistance), -1.0, 1.0));
        float bodyAngle = asin(saturate(body.w / bodyDistance));
        if (separation + targetAngle < bodyAngle) return true;
    }
    return false;
}

Placement placeRock(uint id)
{
    Placement result;
    result.visible = false;
    result.group = 0;
    RockData rock = root.rocks[id];
    CullParams* p = &root.scratch->params;
    uint packed = uint(rock.spin_group.w + .5);
    uint band = packed & (ORBITAL_BELT_BANDS - 1), variant = packed / ORBITAL_BELT_BANDS;
    // Belt frame: spin the band, then tilt and compress the plane.
    float2 spin = p->band_spin[band].xy;
    float3 v = rock.position_radius.xyz;
    float bx = v.x * spin.x - v.z * spin.y, bz = v.x * spin.y + v.z * spin.x;
    float3 local = float3(bx, v.y * p->belt_tilt.x - bz * p->belt_tilt.y, bz * p->belt_tilt.z);
    float3 position = p->giant.xyz + local;
    float radius = rock.position_radius.w;

    float z = dot(position, p->forward.xyz);
    if (z < -radius) return result;
    float pixelRadius = radius * p->view.x / max(z, .1);
    if (pixelRadius < p->billboard.z) return result;
    // Frustum planes with non-unit normals; padding covers jitter and expanded billboards.
    float zc = max(z, 0.0);
    float padding = radius + zc * p->forward.w;
    if (abs(dot(position, p->right.xyz)) > zc * p->right.w + padding * p->view.y) return result;
    if (abs(dot(position, p->up.xyz)) > zc * p->up.w + padding * p->view.z) return result;
    if (occludedByBodies(position, radius)) return result;

    result.visible = true;
    float4 ring = root.frame->belt_ring;
    float radial = (length(v.xz) - ring.x) / (ring.y - ring.x);
    uint composition = rockClass(id, radial);
    float3 colour = rockColour(composition, id);
    if (pixelRadius < p->billboard.y)
    {
        // Sub-pixel rocks become fixed-size billboards whose coverage fades in with their true size.
        float coverage = pixelRadius * pixelRadius / (p->billboard.y * p->billboard.y);
        coverage *= saturate((pixelRadius - p->billboard.z) / p->billboard.z);
        result.group = BILLBOARD_GROUP;
        result.instance.center_radius = float4(position, radius * p->billboard.y / pixelRadius);
        result.instance.rotation_kind = float4(0.0, 0.0, 0.0, KIND_ROCK);
        result.instance.tint = float4(colour, coverage);
        return result;
    }
    uint level = 0;
    if (pixelRadius >= p->levels.x) level = 1;
    if (pixelRadius >= p->levels.y) level = 2;
    if (pixelRadius >= p->levels.z) level = 3;
    if (pixelRadius >= p->levels.w) level = 4;
    if (pixelRadius >= p->billboard.x) level = 5;
    result.group = variant * ORBITAL_ROCK_LEVELS + level;
    float3 rotation = rock.rotation_seed.xyz + rock.spin_group.xyz * p->view.w;
    result.instance.center_radius = float4(position, radius);
    result.instance.rotation_kind = float4(rotation, KIND_ROCK);
    result.instance.tint = float4(colour, float(composition) + rockMaterialSeed(id));
    return result;
}

[shader("compute")]
[numthreads(ORBITAL_CULL_THREADS, 1, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    CullScratch* scratch = root.scratch;
    uint id = thread.x;
    if (root.pass == 1)
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
    Placement placement = placeRock(id);
    if (!placement.visible) return;
    if (root.pass == 0)
        scratch->counts[placement.group].add(1);
    else
    {
        uint slot = scratch->cursors[placement.group].add(1);
        scratch->instances[slot] = placement.instance;
    }
}
