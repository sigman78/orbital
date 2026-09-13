#pragma once
#include "types.slang"
#include "../scene/shadows.slang"
#include "../lib/math.slang"
#include "../lib/noise.slang"
#include "../lib/sampling.slang"

static const float2 GAS_TEXELS = float2(3600.0, 1800.0);
static const float2 GAS_DETAIL_TEXELS = float2(4096.0, 2048.0);
// Texture units per second per unit of the flow map's signed channels, at real wind speed (FLOW_SCALE in tools/bake-flow-map.py).
static const float GAS_FLOW_UNIT = 6.25e-7;
// The slope at the relief map's square-root encoding's full scale (RELIEF_SLOPE_MAX in the bake tool).
static const float GAS_SLOPE_MAX = .1;
// Polar caps: an atlas of two azimuthal projections (north above south) of the cap within
// POLAR_CAP_DEG of each pole, scaled by the panel's cap size and crossfaded into the map over
// the outer part of its radius; the cap flow is in cap texture units per second
// (POLAR_FLOW_SCALE in the bake tool).
static const float GAS_CAP_SIN = .34202;   // sin 20 degrees, the atlas's extent at cap size 1
static const float GAS_POLAR_FLOW_UNIT = 2.5e-6;
static const float2 GAS_POLAR_TEXELS = float2(1024.0, 1024.0);

// The polar cap's albedo at a local normal, advected on the cap's own flow the
// same way as the map: Juno's cyclone clusters, since the map is smeared there.
float3 giantPolar(float3 localN, float2 uv, float2 dx, float2 dy, float t, float4 giant)
{
    // uv is the cap coordinate already scaled by the cap size.
    float south = localN.y < 0.0 ? .5 : 0.0;
    float2 atlas = float2(uv.x, uv.y * .5 + south);
    float2 flow = (textures[TEX_GAS_POLAR_FLOW].SampleLevel(samplers[SAMPLER_CLAMP], atlas, 0.0).rg * 2.0 - 1.0) * GAS_POLAR_FLOW_UNIT * giant.x;
    float cycle = giant.y;
    float phase = fbm2(uv * float2(40, 40) + south * 7.0, 2);
    float p0 = frac(t / cycle + phase), p1 = frac(p0 + .5);
    float w0 = 1.0 - abs(2.0 * p0 - 1.0);
    float2 d0 = -flow * (p0 - .5) * cycle, d1 = -flow * (p1 - .5) * cycle;
    float3 c0 = textures[TEX_GAS_POLAR].SampleGrad(samplers[SAMPLER_CLAMP], atlas + d0 * float2(1.0, .5), dx, dy).rgb;
    float3 c1 = textures[TEX_GAS_POLAR].SampleGrad(samplers[SAMPLER_CLAMP], atlas + d1 * float2(1.0, .5), dx, dy).rgb;
    float fine = magnification(uv, GAS_POLAR_TEXELS) * giant.z;
    float3 colour = lerp(c1, c0, w0);
    return colour * (1.0 + (fbm2((uv + lerp(d1, d0, w0)) * float2(2400, 2400) - t * .003, 2) - .5) * .12 * fine);
}

// Streaks along the local wind from noise: four taps of one noise octave
// averaged along the flow direction, the bake's line integral convolution
// evaluated per pixel at whatever scale the view needs. In standard deviations.
float giantStreaks(float2 uv, float2 along, float frequency)
{
    float sum = 0.0;
    for (int i = 0; i < 4; ++i)
        sum += noise2((uv + along * (float(i) - 1.5) * (.9 / frequency)) * float2(frequency, frequency * .5));
    return (sum * .25 - .5) * 5.0;
}

// Fine structure once the map is magnified so its softness never shows,
// riding the advected coordinate: baked filaments that run with the wind
// (tools/bake-flow-map.py, in standard deviations) and the finest noise
// beyond the 4K they hold. Closer still, where a filament texel would cover
// several pixels and bilinear filtering draws the texels as a comb, the baked
// map fades here and the per-pixel streaks of giantCloseDetail take over.
float giantDetail(float2 uv, float t, float fine, float extreme)
{
    float filaments = (sampleSphere(textures[TEX_GAS_DETAIL], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).r - .5) * 6.0;
    float detail = 1.0 + filaments * .11 * fine * (1.0 - .65 * extreme);
    return detail * (1.0 + (fbm2(uv * float2(2400, 480) - t * .003, 2) - .5) * .12 * fine);
}

// The streaks, once per pixel on the phase-blended advected coordinate (which
// is continuous through the handover, since a phase resets only while its
// weight is zero), a second octave arriving closer in.
float giantCloseDetail(float2 uv, float2 along, float fine, float extreme)
{
    float closer = magnification(uv, GAS_DETAIL_TEXELS * 12.0);
    float detail = 1.0 + giantStreaks(uv, along, 9000.0) * .07 * fine * extreme;
    if (closer > 0.0) detail *= 1.0 + giantStreaks(uv, along, 24000.0) * .05 * fine * closer;
    return detail;
}

// The cloud deck advected along the baked wind map: bands shear past each
// other, the Red Spot turns, the wakes roil. Two phases half a cycle apart
// each displace the albedo backwards along the flow and reset before the
// displacement smears; a triangle weight hands over between them, and a
// per-region phase offset keeps the resets from lining up across the disc.
//
// The cloud tops have relief: zones are high ammonia cloud, belts clearings
// into deeper cloud, the anticyclones stand proud. A baked slope+height map
// (tools/bake-flow-map.py, modelled from the albedo since no height map of
// the clouds exists) is advected with the albedo and tilts the shading normal,
// which shows only where the sun is low. The height in its alpha is not traced
// for shadows: on a height field this smooth the trace's discrete steps drew
// coherent contour bands along the terminator, worst seen from a low angle.
// Two decks: the zones' high ammonia clouds ride above the belts' deeper cloud.
// The upper deck is the albedo displaced toward the viewer by a lift that grows
// with the view's obliquity, masked by the baked relief height, so at oblique
// views the zones slide over the belts; and it shades the deck below it, one
// tap up-sun, so at a low sun the belts darken along the zone edges.
float upperDeck(float2 uv) { return smoothstep(.3, .7, sampleSphere(textures[TEX_GAS_RELIEF], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).a); }

float3 layeredDeck(float2 uv, float2 lift, float2 cast, float2 layers, float t, float fine, float extreme)
{
    float3 lower = sampleSphere(textures[TEX_GAS_ALBEDO], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).rgb * giantDetail(uv, t, fine, extreme);
    if (layers.x <= 0.0) return lower;
    float3 upper = sampleSphere(textures[TEX_GAS_ALBEDO], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv + lift).rgb * giantDetail(uv + lift, t, fine, extreme);
    float upperHere = upperDeck(uv + lift);
    float shade = 1.0 - layers.y * upperDeck(uv + cast) * (1.0 - upperHere);
    return lerp(lower * shade, upper, upperHere);
}

Material giantMaterial(Frame* frame, SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float3 V, float visibility)
{
    float4 giant = frame->giant;
    if (giant.w < .5) giant.x = 0.0; // the panel's flow switch off: the deck and the caps hold still
    float t = frame->camera_time.w;
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float4 flowMap = sampleSphere(textures[TEX_GAS_FLOW], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv);
    float2 flow = (flowMap.rg * 2.0 - 1.0) * GAS_FLOW_UNIT * giant.x;
    float cycle = giant.y;
    float phase = fbm2(uv * float2(60, 30), 2);
    float p0 = frac(t / cycle + phase), p1 = frac(p0 + .5);
    float w0 = 1.0 - abs(2.0 * p0 - 1.0);
    float2 uv0 = uv - flow * (p0 - .5) * cycle, uv1 = uv - flow * (p1 - .5) * cycle;
    float fine = magnification(uv, GAS_TEXELS) * giant.z * (.5 + flowMap.b);
    // The wind's direction in texture space, for the per-pixel streaks (the map's, not the switch's).
    float2 windUV = flowMap.rg * 2.0 - 1.0;
    float2 along = dot(windUV, windUV) > 1e-6 ? normalize(windUV) : float2(1.0, 0.0);
    float3 T, B;
    tangentFrame(localN, rotate, N, T, B);
    float2 layers = frame->giant_layers.xy;
    float3 viewTS = float3(dot(V, T), dot(V, B), max(dot(V, N), .15));
    float3 lightTS = float3(dot(L, T), dot(L, B), max(dot(L, N), .2));
    float2 lift = viewTS.xy / viewTS.z * layers.x;  // toward the viewer, in texture units
    float2 cast = lightTS.xy / lightTS.z * layers.x; // toward the sun
    // Close-range streaks: 1 once a filament texel covers 8 pixels; nothing with the panel's switch off.
    float streaks = frame->giant_layers.z;
    float extreme = streaks > 0.0 ? magnification(uv, GAS_DETAIL_TEXELS * 4.0) : 0.0;
    float3 c0 = layeredDeck(uv0, lift, cast, layers, t, fine, extreme);
    float3 c1 = layeredDeck(uv1, lift, cast, layers, t, fine, extreme);
    Material m;
    m.albedo = lerp(c1, c0, w0);
    if (extreme > 0.0) m.albedo *= giantCloseDetail(lerp(uv1, uv0, w0), along, fine * streaks, extreme);
    // Over the outer part of the cap's radius the map gives way to the baked polar caps.
    float capSin = GAS_CAP_SIN * max(frame->giant_night.z, 1e-3);
    float capEdge = length(localN.xz) / capSin; // 1 at the cap's edge
    float polar = frame->giant_night.z > 0.0
                      ? (1.0 - smoothstep(1.0 - frame->giant_night.w, 1.0, capEdge)) * frame->giant_more.w : 0.0;
    float2 capUV = .5 + .5 * localN.xz / capSin; // and its footprint, taken outside the branch
    float2 capDx = ddx(capUV) * float2(1.0, .5), capDy = ddy(capUV) * float2(1.0, .5);
    if (polar > 0.0) m.albedo = lerp(m.albedo, giantPolar(localN, capUV, capDx, capDy, t, giant), polar);
    m.rough = .95;
    m.water = 0.0;
    m.visibility = visibility * beltShadow(frame, input.worldPosition);
    // The panel exaggerates the physical slopes so the relief reads along the
    // terminator; under a high sun the same tilt would emboss the whole disc,
    // so it fades to a fraction there, where the true slopes are invisible anyway.
    float grazing = 1.0 - smoothstep(.05, .5, dot(N, L));
    float strength = frame->giant_more.z * lerp(.15, 1.0, grazing) * (1.0 - polar);
    float2 encoded = lerp(sampleSphere(textures[TEX_GAS_RELIEF], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv1).xy, sampleSphere(textures[TEX_GAS_RELIEF], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv0).xy, w0) * 2.0 - 1.0;
    float2 slope = encoded * abs(encoded) * GAS_SLOPE_MAX * strength;
    m.N = normalize(N - T * slope.x - B * slope.y);
    return m;
}

