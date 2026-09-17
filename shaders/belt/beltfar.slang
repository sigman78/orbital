#pragma once
#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../scene/view.slang"
#include "../lib/math.slang"
#include "../lib/noise.slang"
#include "../lib/color.slang"
#include "../lib/scattering.slang"
#include "belt.slang"
#include "veil.slang"

// The belt dust and the far-belt disc share these: the medium's constants,
// the sunlight bake over the belt plane, and the far tier's march through
// the slab reading the baked maps. Included by dust.slang (which composites
// the near march and the far tier in one blend) and disc.slang (the bake).
//
// The far tier: dust and rocks as one baked disc. Two maps over the belt
// plane, in the orthographic disc frame of belt_disc_projection: textures[TEX_BELT_DISC_LIGHT]
// carries the sunlight reaching the plane through the belt and the planets'
// shadows; textures[TEX_BELT_DISC_ROCKS] the small rocks' coverage times albedo, splatted by
// discsplat.slang. farBelt marches the slab's thickness so a face-on view
// integrates one or two samples and an edge-on view keeps the belt's visible
// thickness: the analytic density for the dust, the vertical profile applied
// to the rock coverage, the baked sunlight for both.

static const int DUST_STEPS = 16;
static const int DUST_SUN_STEPS = 2;     // sun-ray density samples per view sample of the near march
static const float DUST_DENSITY = .08;   // dust extinction relative to the rock extinction coefficient: a full chord stays translucent
static const float DUST_ALBEDO = .5;     // single-scattering albedo of the medium
static const float3 DUST_COLOUR = float3(.62, .56, .48); // the belt's greyish-brown regolith, finer
static const float DUST_FORWARD = .55;   // Henyey-Greenstein g: fine dust scatters forward
static const float DUST_ISOTROPIC = .35; // share of an isotropic lobe, standing in for multiple scattering and back-scatter
static const float DUST_SLAB = 3.5;      // slab half-height in flared scale heights; the Gaussian is negligible beyond
static const int DISC_STEPS = 12;
static const float ROCK_AMBIENT = .004;  // the splats' ambient share, relative to the sun's irradiance

float dustPhase(float c) { return lerp(henyeyGreenstein(c, DUST_FORWARD), 1.0 / (4.0 * PI), DUST_ISOTROPIC); }

float sceneDepthAt(int2 pixel)
{
    float d = textures[TEX_DEPTH].Load(int3(pixel, 0)).r;
    return isBackgroundDepth(d) ? FAR_Z : linearViewDepth(d);
}

// The dust colour after the panel's tint and saturation.
float3 dustColour(Frame* frame)
{
    DustParams dust = dustParams(frame);
    float3 tinted = DUST_COLOUR * dust.tint;
    return max(lerp(luma709(tinted).xxx, tinted, dust.saturation), 0.0);
}

// The view ray's segment inside the belt slab and outer cylinder, clipped to
// the scene; false when it misses. Also returns the camera's excess height
// above the slab and distance beyond the edge, for the distance ramp.
bool beltSegment(Frame* frame, float3 rd, int2 depthPixel, out float t0, out float t1, out float aboveSlab, out float beyondEdge, out float halfHeight)
{
    aboveSlab = beyondEdge = 0.0; // defined even when the ray misses the slab
    BeltParams belt = beltParams(frame);
    float3 q0 = -belt.centre; // the camera in the giant's frame
    halfHeight = belt.thickness * .5 * 1.4 * DUST_SLAB;
    float hn = dot(q0, belt.normal), dn = dot(rd, belt.normal);
    t0 = 0.0;
    t1 = FAR_Z;
    if (abs(dn) < 1e-5)
    {
        if (abs(hn) > halfHeight) return false;
    }
    else
    {
        t0 = (-halfHeight - hn) / dn;
        t1 = (halfHeight - hn) / dn;
        if (t0 > t1) { float s = t0; t0 = t1; t1 = s; }
    }
    float outer = belt.inner + BELT_OUTER_TAIL * (belt.outer - belt.inner);
    float3 qp = q0 - belt.normal * hn, rp = rd - belt.normal * dn;
    float a = dot(rp, rp), b = 2.0 * dot(qp, rp), c = dot(qp, qp) - outer * outer;
    if (a > 1e-8)
    {
        float disc = b * b - 4.0 * a * c;
        if (disc < 0.0) return false;
        float s = sqrt(disc);
        t0 = max(t0, (-b - s) / (2.0 * a));
        t1 = min(t1, (-b + s) / (2.0 * a));
    }
    t0 = max(t0, 0.0);
    t1 = min(t1, sceneDepthAt(depthPixel) / max(dot(rd, viewParams(frame).forward), 1e-4));
    aboveSlab = abs(hn) - halfHeight;
    beyondEdge = length(qp) - outer;
    return t1 > t0;
}

// Plane-space uv of a camera-relative point.
float2 discUv(Frame* frame, float3 p) { return mul(beltDiscProjection(frame), float4(p, 1.0)).xy * .5 + .5; }

// --- Sunlight bake over the belt plane -------------------------------------------
// R: transmittance through the dust's own thin medium (what the near march
// shades the dust with), G: through the belt's rock extinction (what the
// splats are shaded with), B: the planets' shadows.
float4 bakeSunlight(float2 position)
{
    Frame* frame = root.frame;
    BeltDiscParams disc = beltDiscParams(frame);
    float2 ndc = (position / disc.lightTexels) * 2.0 - 1.0;
    float3 giant = beltParams(frame).centre;
    // Inverse of the orthographic projection: right along +x, up along -y (see write_light_projection).
    float3 point = giant + disc.right * (ndc.x * disc.half.x) - disc.up * (ndc.y * disc.half.y);
    float rockTransmittance = beltExtinction(frame, point);
    float dustTransmittance = pow(rockTransmittance, DUST_DENSITY); // the same optical depth, scaled to the dust
    return float4(dustTransmittance, rockTransmittance, solarVisibility(frame, point, -1), 1.0);
}

// --- The far tier -------------------------------------------------------------------
// Premultiplied radiance and opacity of the far belt along this pixel's ray, unweighted.
float4 farBelt(float2 position)
{
    Frame* frame = root.frame;
    BeltParams belt = beltParams(frame);
    BeltDiscParams disc = beltDiscParams(frame);
    DustParams dustSettings = dustParams(frame);
    LightParams sunLight = lightParams(frame);
    ViewParams view = viewParams(frame);
    float3 rd = rayDirection(root.frame, position);
    float t0, t1, aboveSlab, beyondEdge, halfHeight;
    if (!beltSegment(frame, rd, int2(position), t0, t1, aboveSlab, beyondEdge, halfHeight))
        return float4(0.0, 0.0, 0.0, 0.0);

    // The same distance ramp as the near march, so the handover is seamless.
    float farScale = dustFarScale(aboveSlab, beyondEdge, halfHeight, dustSettings.farScale);
    float3 colour = dustColour(frame);
    float dustDensity = dustSettings.density * farScale, dustBrightness = dustSettings.brightness;

    float jitter = marchJitter(position, view.frame, temporalParams(frame).enabled);
    // The sun is far: one light direction and one phase serve the whole segment.
    float3 light = normalize(sunLight.position - rd * (.5 * (t0 + t1)));
    float dustPhaseValue = dustPhase(dot(light, rd));
    float rockPhase = lambertSpherePhase(acos(clamp(dot(light, -rd), -1.0, 1.0)));
    float ds = (t1 - t0) / float(DISC_STEPS);
    float transmittance = 1.0;
    float3 radiance = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < DISC_STEPS; ++i)
    {
        float t = t0 + (float(i) + jitter) * ds;
        float3 p = rd * t;
        float3 q = p - belt.centre;
        float h = dot(q, belt.normal);
        float r = length(q - belt.normal * h);
        float radial = (r - belt.inner) / (belt.outer - belt.inner);
        float scaleHeight = belt.thickness * .5 * lerp(.6, 1.4, saturate(radial));
        float vertical = exp(-h * h / (2.0 * scaleHeight * scaleHeight));
        float profile = vertical / (2.5066283 * scaleHeight); // normalised: the column integrates to one
        float density = beltDensity(q, belt) * belt.density;
        float2 uv = discUv(frame, p);
        float3 baked = textures[TEX_BELT_DISC_LIGHT].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rgb;
        float dustSunlight = sunLight.radiance * baked.r * baked.b, rockSunlight = sunLight.radiance * baked.g * baked.b;

        float sigmaDust = density * BELT_EXTINCTION * DUST_DENSITY * dustDensity;
        float3 dust = colour * DUST_ALBEDO * dustBrightness * sigmaDust * dustPhaseValue * dustSunlight;

        // The map is splatted pre-blurred (discsplat.slang), so one texel is the smoothed estimate.
        float4 rocks = textures[TEX_BELT_DISC_ROCKS].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0) * profile; // coverage per unit length along the ray
        float3 rock = rocks.rgb * (rockSunlight / PI * rockPhase + sunLight.radiance * ROCK_AMBIENT);

        float sigma = sigmaDust + rocks.a;
        if (sigma < 1e-6) continue;
        float stepTransmittance = exp(-sigma * ds);
        radiance += transmittance * (dust + rock) * (1.0 - stepTransmittance) / sigma;
        transmittance *= stepTransmittance;
        if (transmittance < .005) break;
    }
    return float4(radiance, 1.0 - transmittance);
}
