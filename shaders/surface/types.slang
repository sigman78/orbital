#pragma once
#include "../scene_shared.h"

// Root.mode values (SurfaceMode on the C++ side).
static const uint MODE_OPAQUE = ORBITAL_SURFACE_OPAQUE, MODE_CLOUD = ORBITAL_SURFACE_CLOUD,
                  MODE_SHADOW = ORBITAL_SURFACE_SHADOW, MODE_BILLBOARD = ORBITAL_SURFACE_BILLBOARD,
                  MODE_SPLAT_MASK = ORBITAL_SURFACE_SPLAT_MASK;
// Instance kinds (rotation_kind.w); SurfaceKind on the C++ side.
static const int KIND_EARTH = ORBITAL_KIND_EARTH, KIND_GIANT = ORBITAL_KIND_GIANT, KIND_MOON = ORBITAL_KIND_MOON,
                 KIND_ROCK = ORBITAL_KIND_ROCK, KIND_MARS = ORBITAL_KIND_MARS, KIND_MINOR_PLANET = ORBITAL_KIND_MINOR_PLANET;

struct SurfaceVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPosition : WORLD_POSITION;
    [[vk::location(1)]] float3 worldNormal : WORLD_NORMAL;
    [[vk::location(2)]] float3 localNormal : LOCAL_NORMAL;
    [[vk::location(3)]] nointerpolation uint instanceIndex : INSTANCE_INDEX;
    [[vk::location(4)]] float2 tileUV : TILE_UV;
    [[vk::location(5)]] nointerpolation uint tileSlot : TILE_SLOT;
    [[vk::location(6)]] nointerpolation uint tileFace : TILE_FACE;
    [[vk::location(7)]] float3 bary : BARYCENTRIC;
    [[vk::location(8)]] float patchMorph : PATCH_MORPH; // 0 the patch's own shape and tiles, 1 its parent's
};

struct Material {
    float3 albedo;
    float3 N;      // shading normal
    float rough;
    float water;   // 0 land, 1 ocean
    float visibility;
};

// What every body's fragment starts from: the instance, the geometric normal,
// the view and light directions and the sun's visibility at the point.
struct Surface {
    Instance inst;
    int kind;
    float3 N, V, L;
    float visibility;
};
