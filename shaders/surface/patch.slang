#pragma once
#include "../scene/bindings.slang"
#include "../scene/instances.slang"
#include "../scene/frame_records.slang"
#include "../lib/math.slang"

static const float EVERITT_K = 0.8687;
static const float TAN_K = tan(EVERITT_K);
static const float PATCH_QUADS = 32.0;
static const float TILE_SIDE = 33.0;
static const float TILE_COLOUR_RATIO = MINOR_PLANET_COLOUR_RATIO;
static const float TILE_COLOUR_SIDE = PATCH_QUADS * TILE_COLOUR_RATIO + 1.0;
static const float SKIRT_DROP = MINOR_PLANET_SKIRT_DROP;

static const float3 FACE_AXIS[6] = { float3(1,0,0), float3(-1,0,0), float3(0,1,0), float3(0,-1,0), float3(0,0,1), float3(0,0,-1) };
static const float3 FACE_S[6] = { float3(0,0,-1), float3(0,0,1), float3(1,0,0), float3(1,0,0), float3(1,0,0), float3(-1,0,0) };
static const float3 FACE_T[6] = { float3(0,1,0), float3(0,1,0), float3(0,0,-1), float3(0,0,1), float3(0,1,0), float3(0,1,0) };

float3 patchCubeDirection(uint face, float2 st)
{
    float2 w = tan(EVERITT_K * st) / TAN_K;
    return normalize(FACE_AXIS[face] + FACE_S[face] * w.x + FACE_T[face] * w.y);
}

// The frame a slope tile is written against (scene/terrain_patch.hpp's
// patch_tangent_frame): the face's s direction squared up against the sphere's own
// normal at the point. The reference is the direction, never the face's axis, which
// turns at a cube edge; the face only picks which way is x. FACE_S lies 45 degrees
// off the nearest corner of the face, so the projection never runs short.
void patchTangentFrame(uint face, float3 direction, out float3 tangent, out float3 bitangent)
{
    tangent = normalize(FACE_S[face] - direction * dot(FACE_S[face], direction));
    bitangent = cross(direction, tangent);
}

float patchHeight(uint slot, float2 g)
{
    float2 uv = (g + 0.5) / TILE_SIDE;
    float raw = textureArrays[TEX_ARRAY_TERRAIN_HEIGHT].SampleLevel(samplers[SAMPLER_CLAMP], float3(uv, float(slot)), 0).r;
    return MINOR_PLANET_HEIGHT_MIN + raw * (MINOR_PLANET_HEIGHT_MAX - MINOR_PLANET_HEIGHT_MIN);
}

struct PatchResult {
    float3 localPosition; // body frame, radii, the skirt drop applied
    float3 worldPosition;
    float3 worldNormal;
    float3 localNormal;
    float2 tileUV;
    uint slot;
    uint face;
    float3 bary; // one-hot per triangle corner on the unindexed wireframe grid, zero otherwise
    float morph; // 0 own shape, 1 the parent's
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
    // A quadrant a child draws instead: its triangles go, by a vertex strictly
    // inside it (the ones on the quadrant seams stay for the neighbouring quadrants).
    uint quadrants = (patch.tile[3] >> 6) & 0xf;
    bool2 low = g < PATCH_QUADS * 0.5, high = g > PATCH_QUADS * 0.5;
    uint quadrant = (high.x ? 1 : 0) | (high.y ? 2 : 0);
    bool dropped = (low.x || high.x) && (low.y || high.y) && !((quadrants >> quadrant) & 1);
    float2 cell_origin = patch.cell.xy;
    float cell_size = patch.cell.z;
    uint face = patch.tile[0];
    uint slot = patch.tile[1];

    // Nothing this vertex carries is read: its triangles are all discarded, so it
    // leaves before the two warps and the two height fetches the rest of it costs.
    if (dropped)
    {
        PatchResult dead = {};
        dead.localPosition = float3(asfloat(0x7fc00000), 0, 0); // NaN: the triangle goes
        dead.worldPosition = dead.localPosition;
        dead.localNormal = float3(0, 0, 1);
        dead.worldNormal = float3(0, 0, 1);
        dead.slot = slot;
        dead.face = face | uint(patch.cell.w) << 3 | patch.tile[2] << 8 | patch.tile[3] << 20;
        return dead;
    }

    float2 st = cell_origin + g / PATCH_QUADS * cell_size;
    float3 d = patchCubeDirection(face, st);
    float h = patchHeight(slot, g);
    float3 world = inst.center_radius.xyz + mul(rotate, d) * radius * (1 + h);
    float dist = length(world) / radius;
    // The distance morph, with a floor under it while the tile fades in: a tile
    // just arrived is drawn as its parent's shape and relaxes to its own, so the
    // detail it adds fades in where it would otherwise appear between two frames.
    float m = max(saturate((dist - patch.morph.x) / max(patch.morph.y - patch.morph.x, 1e-6)), patch.morph.z);
    // Sides that meet a finer patch (TerrainTier::Seam::clamp). The finer one is
    // fully morphed to this patch's own shape along them, so this patch has to be
    // standing at that shape too: the morph falls to nothing at such a side and
    // comes back over a quarter of the grid, which is smooth inside the patch and
    // leaves the other three sides to meet their own neighbours as they did.
    uint finer = uint(patch.morph.w);
    if (finer != 0)
    {
        float2 u = g / PATCH_QUADS;
        float inward = 1.0;
        if (finer & 1) inward = min(inward, u.x);
        if (finer & 2) inward = min(inward, 1.0 - u.x);
        if (finer & 4) inward = min(inward, u.y);
        if (finer & 8) inward = min(inward, 1.0 - u.y);
        m *= smoothstep(0.0, 0.25, inward);
    }

    float2 gm = g - frac(g * 0.5) * 2.0 * m;
    st = cell_origin + gm / PATCH_QUADS * cell_size;
    d = patchCubeDirection(face, st);
    h = patchHeight(slot, gm);

    float3 p = d * (1 + h);
    if (skirt > 0.5)
        p = d * (1 + h - SKIRT_DROP);

    PatchResult r;
    r.localPosition = p;
    r.worldPosition = inst.center_radius.xyz + mul(rotate, p) * radius;
    r.worldNormal = normalize(mul(rotate, d));
    r.localNormal = d;
    r.tileUV = gm / PATCH_QUADS;
    r.slot = slot;
    // The face in 0..2, the level in 3..6, the parent's slot in 8..19, then the
    // quadrant and the cube-edge sides.
    r.face = face | uint(patch.cell.w) << 3 | patch.tile[2] << 8 | patch.tile[3] << 20;
    r.bary = grid.normal.xyz;
    r.morph = m;
    return r;
}
