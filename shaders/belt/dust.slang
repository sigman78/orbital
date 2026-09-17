#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../scene/view.slang"
#include "../lib/noise.slang"
#include "beltfar.slang"

// Belt dust: the particulate matter between the rocks, too fine to draw, as a
// scattering medium. The view ray is marched through the same analytic belt
// density the rock extinction uses (rings, gap, flared height), so the dust
// glows where the rocks are dense and thins at the edges; each sample takes
// sunlight through the belt, the planets' shadows and the rock transmittance
// map, and scatters it with a forward-peaked phase, so the belt lights up
// against the sun and stays a faint haze away from it. What lies behind is
// attenuated by the view transmittance (premultiplied output).
//
// Mode 0 marches at half resolution into textures[TEX_BELT_DUST], with the ray and the
// scene depth taken at the even full-resolution pixel of each block. Mode 1
// composites the frame: the march upsampled with a depth-aware 4-tap filter
// so the haze does not bleed across rock silhouettes, mixed with the far-belt
// disc (beltfar.slang) by the LOD weight in a single blend, so the belt's
// opacity crosses over linearly rather than dipping mid-fade as two stacked
// layers would. The ray start is jittered per pixel and frame (the frame count,
// so a paused simulation keeps decorrelating it), and the
// temporal pass settles the noise. Frame.quality.z switches the dust off;
// the far disc still draws its rock layer then.

static const uint MODE_MARCH = 0, MODE_COMPOSITE = 1;
static const float UPSAMPLE_DEPTH_TOLERANCE = .05; // relative depth difference at which a tap loses half its weight

// Sunlight left at a belt point through the dust itself: as beltExtinction
// but for the thin medium and with fewer samples, since the view march
// integrates over many of them.
float sunTransmittance(float3 q, float3 light, BeltParams belt)
{
    float climb = max(abs(dot(light, belt.normal)), .08);
    float length = min((belt.thickness * .5 * 1.3 + abs(dot(q, belt.normal))) / climb, belt.outer - belt.inner);
    float ds = length / float(DUST_SUN_STEPS);
    float tau = 0.0;
    for (int i = 0; i < DUST_SUN_STEPS; ++i)
        tau += beltDensity(q + light * ((float(i) + .5) * ds), belt) * ds;
    return exp(-tau * belt.density * BELT_EXTINCTION * DUST_DENSITY);
}

float4 march(float2 fullResPixel, int2 depthPixel)
{
    Frame* frame = root.frame;
    BeltParams belt = beltParams(frame);
    DustParams dustSettings = dustParams(frame);
    LightParams sunLight = lightParams(frame);
    ViewParams view = viewParams(frame);
    float3 rd = rayDirection(root.frame, fullResPixel);
    float t0, t1, aboveSlab, beyondEdge, halfHeight;
    if (!beltSegment(frame, rd, depthPixel, t0, t1, aboveSlab, beyondEdge, halfHeight))
        return float4(0.0, 0.0, 0.0, 0.0);
    float farScale = dustFarScale(aboveSlab, beyondEdge, halfHeight, dustSettings.farScale);
    float3 colour = dustColour(frame);

    float jitter = hash(float3(fullResPixel, view.frame * 13.0 + view.jitter.x * 37.0));
    float ds = (t1 - t0) / float(DUST_STEPS);
    float transmittance = 1.0;
    float3 radiance = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < DUST_STEPS; ++i)
    {
        float t = t0 + (float(i) + jitter) * ds;
        float3 p = rd * t;
        float3 q = p - belt.centre;
        float density = beltDensity(q, belt) * belt.density * farScale;
        if (density < 1e-5) continue;
        float sigma = density * BELT_EXTINCTION * DUST_DENSITY * dustSettings.density; // extinction per unit length
        float3 light = normalize(sunLight.position - p);
        float sunlight = sunLight.radiance * sunTransmittance(q, light, belt) * solarVisibility(frame, p, -1)
                       * beltRockTransmittance(frame, p);
        float3 inscatter = colour * DUST_ALBEDO * dustSettings.brightness * sigma * dustPhase(dot(light, rd)) * sunlight;
        // Analytic integral of the in-scatter over the step under constant density.
        float stepTransmittance = exp(-sigma * ds);
        radiance += transmittance * inscatter * (1.0 - stepTransmittance) / sigma;
        transmittance *= stepTransmittance;
        if (transmittance < .005) break;
    }
    return float4(radiance, 1.0 - transmittance);
}

// Depth-aware upsample of the half-resolution march: the four taps around
// the pixel, bilinear weights scaled by how close each tap's scene depth is
// to this pixel's, so dust in front of a rock does not smear over its edge.
float4 upsampleNear(float2 position)
{
    int2 pixel = int2(position);
    float depth = sceneDepthAt(pixel);
    float2 halfSize = floor(viewParams(root.frame).size * .5);
    float2 coordinate = position * .5 - .5;
    float2 base = floor(coordinate);
    float2 f = coordinate - base;
    float4 sum = float4(0.0, 0.0, 0.0, 0.0), fallback = float4(0.0, 0.0, 0.0, 0.0);
    float weightSum = 0.0;
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i)
        {
            int2 tap = clamp(int2(base) + int2(i, j), int2(0, 0), int2(halfSize) - 1);
            float bilinear = (i == 0 ? 1.0 - f.x : f.x) * (j == 0 ? 1.0 - f.y : f.y);
            float4 dust = textures[TEX_BELT_DUST].Load(int3(tap, 0));
            float tapDepth = sceneDepthAt(tap * 2);
            float relative = abs(tapDepth - depth) / max(min(tapDepth, depth), 1e-3);
            float weight = bilinear / (1.0 + relative / UPSAMPLE_DEPTH_TOLERANCE);
            sum += dust * weight;
            weightSum += weight;
            fallback += dust * bilinear;
        }
    return weightSum > 1e-4 ? sum / weightSum : fallback;
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    Frame* frame = root.frame;
    BeltParams belt = beltParams(frame);
    if (belt.density <= 0.0) return float4(0.0, 0.0, 0.0, 0.0);
    DustParams dust = dustParams(frame);
    float weight = beltDiscParams(frame).weight;
    if (root.mode == MODE_MARCH)
    {
        int2 depthPixel = int2(position.xy) * 2;
        return march(float2(depthPixel) + .5, depthPixel);
    }
    // One blend: the near march and the far disc mixed by the LOD weight.
    float4 near = dust.enabled && weight < 1.0 ? upsampleNear(position.xy) : float4(0.0, 0.0, 0.0, 0.0);
    float4 far = weight > 0.0 ? farBelt(position.xy) : float4(0.0, 0.0, 0.0, 0.0);
    return near * (1.0 - weight) + far * weight;
}
