#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../scene/bindings.slang"
#include "../surface/lighting.slang"
#include "../surface/brdf.slang"
#include "../surface/patch.slang"
#include "../lib/math.slang"
#include "../lib/color.slang"
#include "../lib/noise.slang"
#include "../lib/sampling.slang"

// Moon/Mars DEMs and procedural minor-planet maps share normal+height sampling.
// root.detail fades normal tilt and terrain shadows with draw size; grain fades
// with texture magnification.

static const float2 DEM_TEXELS = float2(4096.0, 2048.0);     // Moon and Mars albedo and normal+height maps
static const float2 MINOR_PLANET_TEXELS = float2(2048.0, 1024.0);  // the minor planet's, baked at start-up
// Height range of the baked DEM alpha channel in radii (manifest height_min_m..height_max_m over the body radius).
static const float MOON_HEIGHT_RANGE = 19603.0 / 1737400.0;
static const float MARS_HEIGHT_RANGE = 29334.0 / 3396200.0;
static const float MINOR_PLANET_HEIGHT_RANGE = MINOR_PLANET_HEIGHT_MAX - MINOR_PLANET_HEIGHT_MIN;

// Trace toward the sun through DEM alpha. heightRange converts to radii;
// cosLatitude corrects the longitude step near the poles.
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

// Shared normal blend for sphere and patch paths; bound the upright component
// so scaled tilt cannot turn the shading normal below the surface.
float3 blendMapNormal(float3 axis, float3 tangential, float upright, float strength, float tier)
{
    return normalize(axis * lerp(1.0, max(upright, .2), tier) + tangential * (strength * tier));
}

// Add grain under magnification, using the same sphere coordinates in both paths.
float3 regolithGrain(float3 albedo, float2 sphere_uv, float magnified)
{
    if (magnified <= 0.0)
        return albedo;
    return albedo * (1.0 + (fbm2(sphere_uv * float2(3072, 1536), 3) - .5) * .10 * magnified);
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
        m.N = blendMapNormal(N, T * mapN.x + B * mapN.y, mapN.z, strength, tier);
        m.visibility *= terrainShadow(normalMap, texels, uv, L, N, T, B, heightRange, length(localN.xz), tier);
    }
    m.albedo = regolithGrain(sampleSphere(albedoMap, samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).rgb, uv, detail);
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

// Blend tile albedo and tangent slopes toward the parent through the morph band.
// Parent and child use the same tangent frame; cube faces decode into body space.
Material patchMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility, float tier)
{
    uint slot = input.tileSlot, face = input.tileFace & 7, parent = (input.tileFace >> 8) & 0xfff, quadrant = (input.tileFace >> 20) & 3;
    float morph = input.patchMorph;
    float3 localN = normalize(input.localNormal);
    float2 uv = (input.tileUV * PATCH_QUADS * TILE_COLOUR_RATIO + 0.5) / TILE_COLOUR_SIDE;
    float3 albedo = textureArrays[TEX_ARRAY_TERRAIN_ALBEDO].SampleLevel(samplers[SAMPLER_CLAMP], float3(uv, float(slot)), 0).rgb;
    float2 slope = textureArrays[TEX_ARRAY_TERRAIN_SLOPE].SampleLevel(samplers[SAMPLER_CLAMP], float3(uv, float(slot)), 0).xy * 2.0 - 1.0;
    if (morph > 0.0 && parent < ORBITAL_TERRAIN_SLOTS)
    {
        float2 parentUV = ((float2(quadrant & 1, quadrant >> 1) + input.tileUV) * 0.5 * PATCH_QUADS * TILE_COLOUR_RATIO + 0.5) / TILE_COLOUR_SIDE;
        float3 parentAlbedo = textureArrays[TEX_ARRAY_TERRAIN_ALBEDO].SampleLevel(samplers[SAMPLER_CLAMP], float3(parentUV, float(parent)), 0).rgb;
        float2 parentSlope = textureArrays[TEX_ARRAY_TERRAIN_SLOPE].SampleLevel(samplers[SAMPLER_CLAMP], float3(parentUV, float(parent)), 0).xy * 2.0 - 1.0;
        albedo = lerp(albedo, parentAlbedo, morph);
        slope = lerp(slope, parentSlope, morph);
    }
    float3 tangent, bitangent;
    patchTangentFrame(face, localN, tangent, bitangent);
    // Convert the blended tangent gradient to a shading normal.
    float3 tilt = (tangent * slope.x + bitangent * slope.y) * -MINOR_PLANET_SLOPE_SCALE;
    float upright = rsqrt(1.0 + dot(tilt, tilt));
    Material m;
    m.N = normalize(mul(rotate, blendMapNormal(localN, tilt * upright, upright, 1.0, tier)));
    m.visibility = visibility;
    if (tier > 0.0)
    {
        // Trace the continuous equirectangular map to avoid tile-boundary shadow seams.
        float3 T, B;
        tangentFrame(localN, rotate, N, T, B);
        m.visibility *= terrainShadow(textures[TEX_MINOR_PLANET_NORMAL], MINOR_PLANET_TEXELS, sphereUV(localN), L, N, T, B,
                                      MINOR_PLANET_HEIGHT_RANGE, length(localN.xz), tier);
    }
    // Match the sphere path's grain when tile texels are magnified.
    float2 tileTexels = float2(TILE_COLOUR_SIDE, TILE_COLOUR_SIDE);
    m.albedo = regolithGrain(albedo, sphereUV(localN), magnificationGrad(ddx(uv), ddy(uv), tileTexels));
    m.rough = .95;
    m.water = 0.0;
    return m;
}

// Wireframe from triangle barycentrics, with separate patch and cube-edge colours.
// Sunlight scales line brightness to follow exposure.
float3 wireframe(float3 colour, SurfaceVaryings input, float3 irradiance)
{
    float3 edge = input.bary / max(fwidth(input.bary), 1e-6);
    float lineTriangle = 1.0 - smoothstep(0.0, 1.0, min(edge.x, min(edge.y, edge.z)));
    float2 border = min(input.tileUV, 1.0 - input.tileUV) / max(fwidth(input.tileUV), 1e-6);
    float linePatch = 1.0 - smoothstep(0.0, 1.5, min(border.x, border.y));
    // The sides on a cube edge, in a third colour.
    uint sides = (input.tileFace >> 22) & 0xf;
    float2 lowSide = input.tileUV / max(fwidth(input.tileUV), 1e-6), highSide = (1.0 - input.tileUV) / max(fwidth(input.tileUV), 1e-6);
    float cubeEdge = 1e9;
    if (sides & 1) cubeEdge = min(cubeEdge, lowSide.x);
    if (sides & 2) cubeEdge = min(cubeEdge, highSide.x);
    if (sides & 4) cubeEdge = min(cubeEdge, lowSide.y);
    if (sides & 8) cubeEdge = min(cubeEdge, highSide.y);
    float lineCube = 1.0 - smoothstep(0.0, 2.0, cubeEdge);
    float bright = max(luma709(irradiance) * .04, 1e-4);
    colour = lerp(colour, float3(.15, 1.0, .35) * bright, lineTriangle * .8);
    colour = lerp(colour, float3(1.0, .75, .2) * bright, linePatch);
    return lerp(colour, float3(1.0, .15, .1) * bright, lineCube);
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Surface s = surfaceAt(root.frame, root.instances, input, true);
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    Material m = (root.flags & ORBITAL_ROOT_PATCHES) ? patchMaterial(input, rotate, s.N, s.L, s.visibility, root.detail)
               : s.kind == KIND_MARS                ? marsMaterial(input, rotate, s.N, s.L, s.visibility, root.detail)
               : s.kind == KIND_MINOR_PLANET         ? minorPlanetMaterial(input, rotate, s.N, s.L, s.visibility, root.detail)
                                                    : moonMaterial(input, rotate, s.N, s.L, s.visibility, root.detail);
    float3 irradiance = sunlight(root.frame);
    // Isolate patch attributes to diagnose seams (TerrainSettings::debug).
    uint debug = uint(root.frame->sequence.y + .5);
    if ((root.flags & ORBITAL_ROOT_PATCHES) && debug != 0)
    {
        float3 v = 0.0;
        if (debug == 1)      // the tile coordinate as a checker of 8 cells, the slot's hue as a tint
        {
            float2 cells = floor(input.tileUV * 8.0);
            float checker = fmod(cells.x + cells.y, 2.0);
            float hue = frac(float(input.tileSlot) * .618034);
            v = lerp(float3(.15, .15, .15), float3(.9, .9, .9), checker) * (0.6 + 0.4 * float3(hue, 1.0 - hue, .5));
        }
        else if (debug == 2) v = m.N * 0.5 + 0.5;                       // the shading normal, world frame
        else if (debug == 3)                                             // elevation over the height range
        {
            float h = patchHeight(input.tileSlot, input.tileUV * PATCH_QUADS);
            v = (h - MINOR_PLANET_HEIGHT_MIN) / MINOR_PLANET_HEIGHT_RANGE;
        }
        else if (debug == 4) v = s.visibility > 0.0 ? m.visibility / s.visibility : 0.0; // the crater-shadow term alone
        else if (debug == 5)                                             // morph in red, the parent present in green, the level's parity in blue
            v = float3(input.patchMorph, ((input.tileFace >> 8) & 0xfff) < ORBITAL_TERRAIN_SLOTS ? 1.0 : 0.0, float((input.tileFace >> 3) & 1));
        return float4(v * 0.18, 1); // about the albedo's level, so the exposure leaves it readable
    }
    float3 colour = shadeSurface(m, s.L, s.V, irradiance, earthParams(root.frame).glint, lightParams(root.frame).ambientFill);
    if ((root.flags & (ORBITAL_ROOT_WIREFRAME | ORBITAL_ROOT_PATCHES)) == (ORBITAL_ROOT_WIREFRAME | ORBITAL_ROOT_PATCHES))
        colour = wireframe(colour, input, irradiance);
    return float4(colour, 1);
}
