#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../surface/lighting.slang"
#include "../surface/brdf.slang"
#include "../lib/math.slang"
#include "../lib/color.slang"
#include "../lib/noise.slang"
#include "../lib/sampling.slang"

// The airless bodies: the Moon and Mars, colour maps over NASA elevation
// models baked into normal+height maps, and the minor planet, whose maps are
// baked at start-up from its generated terrain (scene/terrain.hpp) in the same
// layout; a short height trace gives the crater walls their self-shadowing.
//
// Draw tier (root.detail, set per draw from the projected size): the normal
// map's tilt and the height trace fade out toward the smallest mesh level and
// are skipped at zero, while the albedo and the lighting stay as they are at
// every size; the regolith grain is scaled by the map's magnification, which
// is zero out there, and its noise is skipped where that is.

static const float2 DEM_TEXELS = float2(4096.0, 2048.0);     // Moon and Mars albedo and normal+height maps
static const float2 MINOR_PLANET_TEXELS = float2(2048.0, 1024.0);  // the minor planet's, baked at start-up
// Height range of the baked DEM alpha channel in radii (manifest height_min_m..height_max_m over the body radius).
static const float MOON_HEIGHT_RANGE = 19603.0 / 1737400.0;
static const float MARS_HEIGHT_RANGE = 29334.0 / 3396200.0;
static const float MINOR_PLANET_HEIGHT_RANGE = MINOR_PLANET_HEIGHT_MAX - MINOR_PLANET_HEIGHT_MIN;

// Short shadow trace through the DEM height stored in the normal map's alpha:
// marches up-sun in texture space and darkens where terrain rises above the
// light ray. heightRange converts alpha to radii; cosLatitude widens the
// longitude step where the equirectangular texels narrow toward the poles.
float terrainShadow(Texture2D<float4> normalMap, float2 texels, float2 uv, float3 L, float3 N, float3 T, float3 B,
                   float heightRange, float cosLatitude, float tier)
{
    float2 lightUV = float2(dot(L, T), dot(L, B));
    float lightRise = max(dot(L, N), .02) / max(length(lightUV), 1e-4); // rise per unit of tangent distance
    lightUV = normalize(lightUV + 1e-6);
    float2 texel = float2(1.0 / max(cosLatitude, .05), 1.0) / texels;
    float arc = 2.0 * PI / texels.x; // tangent distance of one equatorial texel, in radii
    float h0 = normalMap.SampleLevel(samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv, 1.0).a * heightRange;
    float shadow = 0.0;
    for (int s = 1; s <= 6; ++s)
    {
        float distance = float(s) * 1.5;
        float h = normalMap.SampleLevel(samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv + lightUV * texel * distance, 1.0).a * heightRange;
        float rise = h - h0 - lightRise * distance * arc;
        shadow = max(shadow, rise / (distance * arc) * 6.0);
    }
    return 1.0 - saturate(shadow) * .55 * tier;
}

Material terrainMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility,
                        Texture2D<float4> albedoMap, Texture2D<float4> normalMap, float2 texels, float heightRange,
                        float strength, float tier)
{
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float detail = magnification(uv, texels);
    float3 T, B;
    tangentFrame(localN, rotate, N, T, B);
    Material m;
    m.N = N;
    m.visibility = visibility;
    if (tier > 0.0)
    {
        float3 mapN = sampleSphere(normalMap, samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).xyz * 2.0 - 1.0;
        float tilt = strength * tier;
        m.N = normalize(T * mapN.x * tilt + B * mapN.y * tilt + N * lerp(1.0, max(mapN.z, .2), tier));
        m.visibility *= terrainShadow(normalMap, texels, uv, L, N, T, B, heightRange, length(localN.xz), tier);
    }
    m.albedo = sampleSphere(albedoMap, samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).rgb;
    // Regolith grain where the 4K maps are magnified.
    if (detail > 0.0) m.albedo *= 1.0 + (fbm2(uv * float2(3072, 1536), 3) - .5) * .10 * detail;
    m.rough = .95;
    m.water = 0.0;
    return m;
}

Material moonMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility, float tier)
{
    return terrainMaterial(input, rotate, N, L, visibility, textures[TEX_MOON_ALBEDO], textures[TEX_MOON_NORMAL], DEM_TEXELS, MOON_HEIGHT_RANGE, 1.0, tier);
}

Material minorPlanetMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility, float tier)
{
    return terrainMaterial(input, rotate, N, L, visibility, textures[TEX_MINOR_PLANET_ALBEDO], textures[TEX_MINOR_PLANET_NORMAL], MINOR_PLANET_TEXELS, MINOR_PLANET_HEIGHT_RANGE, 1.0, tier);
}

Material marsMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility, float tier)
{
    // MOLA slopes are gentle at 5 km per texel; doubled so the relief reads.
    Material m = terrainMaterial(input, rotate, N, L, visibility, textures[TEX_MARS_ALBEDO], textures[TEX_MARS_NORMAL], DEM_TEXELS, MARS_HEIGHT_RANGE, 2.0, tier);
    m.rough = .9;
    return m;
}

// The near tier's review: the patch's quad grid over the surface, in the cube
// face's cell coordinates of scene/terrain_patch.hpp (the face by the major
// axis, s and t through the tangent warp), the patch borders in a second
// colour. Lines are a pixel and a half wide from the coordinates' derivatives,
// and they take their brightness from the sunlight so they read at any exposure.
static const float3 CUBE_AXIS[6] = { float3(1, 0, 0), float3(-1, 0, 0), float3(0, 1, 0), float3(0, -1, 0), float3(0, 0, 1), float3(0, 0, -1) };
static const float3 CUBE_S[6] = { float3(0, 0, -1), float3(0, 0, 1), float3(1, 0, 0), float3(1, 0, 0), float3(1, 0, 0), float3(-1, 0, 0) };
static const float3 CUBE_T[6] = { float3(0, 1, 0), float3(0, 1, 0), float3(0, 0, -1), float3(0, 0, 1), float3(0, 1, 0), float3(0, 1, 0) };

float3 wireframe(float3 colour, float3 n, uint level, float3 irradiance)
{
    float3 a = abs(n);
    int face = a.x >= a.y && a.x >= a.z ? (n.x > 0 ? 0 : 1) : a.y >= a.z ? (n.y > 0 ? 2 : 3) : (n.z > 0 ? 4 : 5);
    float axis = dot(n, CUBE_AXIS[face]);
    float2 st = float2(atan2(dot(n, CUBE_S[face]), axis), atan2(dot(n, CUBE_T[face]), axis)) * (4.0 / PI);
    float2 quads = st * (8.0 * exp2(float(level)));  // the quads across the face at this level, halved for the [-1, 1] span
    float2 cell = frac(quads), patch = frac(quads / 16.0);
    float2 lineQuad = 1.0 - smoothstep(0.0, fwidth(quads) * 1.5, min(cell, 1.0 - cell));
    float2 linePatch = 1.0 - smoothstep(0.0, fwidth(quads / 16.0) * 1.5, min(patch, 1.0 - patch));
    float bright = max(luma709(irradiance) * .04, 1e-4);
    colour = lerp(colour, float3(.15, 1.0, .35) * bright, max(lineQuad.x, lineQuad.y) * .8);
    return lerp(colour, float3(1.0, .75, .2) * bright, max(linePatch.x, linePatch.y));
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Surface s = surfaceAt(root.frame, root.instances, input, true);
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    Material m = s.kind == KIND_MARS        ? marsMaterial(input, rotate, s.N, s.L, s.visibility, root.detail)
               : s.kind == KIND_MINOR_PLANET   ? minorPlanetMaterial(input, rotate, s.N, s.L, s.visibility, root.detail)
                                            : moonMaterial(input, rotate, s.N, s.L, s.visibility, root.detail);
    float3 irradiance = sunlight(root.frame);
    float3 colour = shadeSurface(m, s.L, s.V, irradiance, earthParams(root.frame).glint, lightParams(root.frame).ambientFill);
    if (root.flags & ORBITAL_ROOT_WIREFRAME) {
        uint wfLevel = (root.flags & ORBITAL_ROOT_PATCHES) ? input.patchLevel : root.flags >> 8;
        colour = wireframe(colour, normalize(input.localNormal), wfLevel, irradiance);
    }
    return float4(colour, 1);
}
