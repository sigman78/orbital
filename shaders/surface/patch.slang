#pragma once
#include "../scene/bindings.slang"
#include "../scene/instances.slang"
#include "../scene/frame_records.slang"
#include "../lib/math.slang"

static const float EVERITT_K = 0.8687;
static const float TAN_K = tan(EVERITT_K);
static const float PATCH_QUADS = 64.0;
static const float TILE_SIDE = 65.0;
static const float SKIRT_DROP = 0.002;

static const float3 FACE_AXIS[6] = { float3(1,0,0), float3(-1,0,0), float3(0,1,0), float3(0,-1,0), float3(0,0,1), float3(0,0,-1) };
static const float3 FACE_S[6] = { float3(0,0,-1), float3(0,0,1), float3(1,0,0), float3(1,0,0), float3(1,0,0), float3(-1,0,0) };
static const float3 FACE_T[6] = { float3(0,1,0), float3(0,1,0), float3(0,0,-1), float3(0,0,1), float3(0,1,0), float3(0,1,0) };

float3 patchCubeDirection(uint face, float2 st)
{
    float2 w = tan(EVERITT_K * st) / TAN_K;
    return normalize(FACE_AXIS[face] + FACE_S[face] * w.x + FACE_T[face] * w.y);
}

float patchHeight(uint slot, float2 g)
{
    float2 uv = (g + 0.5) / TILE_SIDE;
    float raw = textureArrays[TEX_ARRAY_TERRAIN_HEIGHT].SampleLevel(samplers[SAMPLER_CLAMP], float3(uv, float(slot)), 0).r;
    return MINOR_PLANET_HEIGHT_MIN + raw * (MINOR_PLANET_HEIGHT_MAX - MINOR_PLANET_HEIGHT_MIN);
}

struct PatchResult {
    float3 worldPosition;
    float3 worldNormal;
    float3 localNormal;
    uint level;
};

PatchResult evaluatePatch(uint vertexIndex, uint instanceIndex, Root root)
{
    Vertex grid = root.vertices[vertexIndex];
    PatchInstance patch = root.patches[instanceIndex];
    Instance inst = loadInstance(root.frame, root.instances, root.base, false);
    float radius = inst.center_radius.w;
    float3x3 rotate = rotation(inst.rotation_kind.xyz);

    float2 g = grid.position.xy;
    float skirt = grid.position.z;
    float2 cell_origin = patch.cell.xy;
    float cell_size = patch.cell.z;
    uint level = uint(patch.cell.w);
    uint face = patch.tile[0];
    uint slot = patch.tile[1];

    float2 st = cell_origin + g / PATCH_QUADS * cell_size;
    float3 d = patchCubeDirection(face, st);
    float h = patchHeight(slot, g);
    float3 world = inst.center_radius.xyz + mul(rotate, d) * radius * (1 + h);
    float dist = length(world) / radius;
    float m = saturate((dist - patch.morph.x) / max(patch.morph.y - patch.morph.x, 1e-6));

    float2 gm = g - frac(g * 0.5) * 2.0 * m;
    st = cell_origin + gm / PATCH_QUADS * cell_size;
    d = patchCubeDirection(face, st);
    h = patchHeight(slot, gm);

    float3 p = d * (1 + h);
    if (skirt > 0.5)
        p = d * (1 + h - SKIRT_DROP);

    PatchResult r;
    r.worldPosition = inst.center_radius.xyz + mul(rotate, p) * radius;
    r.worldNormal = normalize(mul(rotate, d));
    r.localNormal = d;
    r.level = level;
    return r;
}
