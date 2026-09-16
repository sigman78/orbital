#include "../scene/frame_bindings.slang"
#include "../surface/lighting.slang"
#include "../surface/brdf.slang"
#include "../lib/math.slang"
#include "../lib/noise.slang"
#include "../lib/sampling.slang"

// The airless bodies, the Moon and Mars: colour maps over NASA elevation
// models baked into normal+height maps, with a short height trace for the
// crater walls' self-shadowing.
//
// Draw tier (root.detail, set per draw from the projected size): the normal
// map's tilt and the height trace fade out toward the smallest mesh level and
// are skipped at zero, while the albedo and the lighting stay as they are at
// every size; the regolith grain is scaled by the map's magnification, which
// is zero out there, and its noise is skipped where that is.

static const float2 RELIEF_TEXELS = float2(4096.0, 2048.0); // Moon and Mars albedo and normal+height maps
// Height range of the baked DEM alpha channel in radii (manifest height_min_m..height_max_m over the body radius).
static const float MOON_HEIGHT_RANGE = 19603.0 / 1737400.0;
static const float MARS_HEIGHT_RANGE = 29334.0 / 3396200.0;

// Short shadow trace through the DEM height stored in the normal map's alpha:
// marches up-sun in texture space and darkens where terrain rises above the
// light ray. heightRange converts alpha to radii.
float reliefShadow(Texture2D<float4> normalMap, float2 uv, float3 L, float3 N, float3 T, float3 B, float heightRange,
                   float tier)
{
    float2 lightUV = float2(dot(L, T), dot(L, B));
    float lightRise = max(dot(L, N), .02) / max(length(lightUV), 1e-4); // rise per unit of tangent distance
    lightUV = normalize(lightUV + 1e-6);
    float2 texel = 1.0 / RELIEF_TEXELS;
    float arc = 2.0 * PI / RELIEF_TEXELS.x; // tangent distance of one texel, in radii
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

Material reliefMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility,
                        Texture2D<float4> albedoMap, Texture2D<float4> normalMap, float heightRange, float strength,
                        float tier)
{
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float detail = magnification(uv, RELIEF_TEXELS);
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
        m.visibility *= reliefShadow(normalMap, uv, L, N, T, B, heightRange, tier);
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
    return reliefMaterial(input, rotate, N, L, visibility, textures[TEX_MOON_ALBEDO], textures[TEX_MOON_NORMAL], MOON_HEIGHT_RANGE, 1.0, tier);
}

Material marsMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility, float tier)
{
    // MOLA slopes are gentle at 5 km per texel; doubled so the relief reads.
    Material m = reliefMaterial(input, rotate, N, L, visibility, textures[TEX_MARS_ALBEDO], textures[TEX_MARS_NORMAL], MARS_HEIGHT_RANGE, 2.0, tier);
    m.rough = .9;
    return m;
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Surface s = surfaceAt(root.frame, root.instances, input, true);
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    Material m = s.kind == KIND_MARS ? marsMaterial(input, rotate, s.N, s.L, s.visibility, root.detail)
                                     : moonMaterial(input, rotate, s.N, s.L, s.visibility, root.detail);
    return float4(shadeSurface(m, s.L, s.V, sunlight(root.frame), root.frame->earth.y, root.frame->quality.w), 1);
}
