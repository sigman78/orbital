#pragma once
#include "scene/bindings.slang"
#include "scene/visibility.slang"

// Belt lighting shared by the surface shaders and the culling pass, which
// lights the sub-pixel splats once per rock. Dependencies are declared here;
// frame-dependent functions take the frame explicitly.

static const float BELT_OUTER_TAIL = 1.5; // geometry::belt_outer_tail: the density tail ends here, in belt widths past the inner edge

// Relative rock density across the belt (0 inner edge, 1 outer edge); mirrors
// geometry::belt_ring_density: rings, a gap, a sharp inner edge and a fading tail.
float beltRingDensity(float t)
{
    const float innerEdge = .04, outerTail = .14;
    if (t <= -innerEdge) return 0.0;
    float bands = .5 + .5 * cos(2.0 * 3.141592653589793 * (saturate(t) * 3.0 + .15));
    float gap = (t - .58) / .035;
    float core = (.5 + .5 * bands) * (1.0 - .7 * exp(-gap * gap));
    float edgeIn = smoothstep(-innerEdge, innerEdge, t);
    float edgeOut = t > 1.0 ? exp(-(t - 1.0) / outerTail) : 1.0;
    return core * edgeIn * edgeOut;
}

// Belt as a translucent slab: relative rock and dust density at a point given
// in the giant's frame (height above the belt plane, distance from the axis).
static const float BELT_EXTINCTION = .4;     // optical depth per unit path at density 1; the far side of a full-thickness path loses about half its light
static const int BELT_EXTINCTION_STEPS = 8;

float beltDensity(float3 q, float4 ring, float3 n)
{
    float h = dot(q, n);
    float r = length(q - n * h);
    float t = (r - ring.x) / (ring.y - ring.x);
    // Gaussian in height with the scale height flaring outward, as the population does.
    float scaleHeight = ring.w * .5 * lerp(.6, 1.4, saturate(t));
    float vertical = exp(-h * h / (2.0 * scaleHeight * scaleHeight));
    return vertical * beltRingDensity(t);
}

// Sunlight left after crossing the belt to reach a point inside it: the
// density is integrated along the sun ray until the ray leaves the slab.
// Rocks on the sunward rim stay bright, rocks buried deep or on the far side
// sit in a smooth dusk. No texels, so nothing shimmers.
float beltExtinction(Frame* frame, float3 point)
{
    float4 ring = frame->belt_ring;
    if (ring.z <= 0.0 || frame->scene.w < .5) return 1.0;
    float3 n = frame->belt_normal.xyz;
    float3 q = point - frame->bodies[int(frame->scene.y)].xyz;
    float3 light = normalize(frame->sun.xyz - point);
    // Distance until the ray has left the slab's height band, capped for the near-grazing case.
    float climb = max(abs(dot(light, n)), .08);
    float length = min((ring.w * .5 * 1.3 + abs(dot(q, n))) / climb, ring.y - ring.x);
    float ds = length / float(BELT_EXTINCTION_STEPS);
    float tau = 0.0;
    for (int i = 0; i < BELT_EXTINCTION_STEPS; ++i)
        tau += beltDensity(q + light * ((float(i) + .5) * ds), ring, n) * ds;
    return exp(-tau * ring.z * BELT_EXTINCTION);
}

// Sunlight left after the rocks between a belt point and the sun: the
// transmittance map holds per-texel coverage in four slices along the sun ray
// (see beltsplat.slang); the slices on the sun side of the point attenuate it.
static const float BELT_MAP_OPACITY = 1.0; // optical depth per unit of texel coverage
static const float BELT_MAP_SLICES = 4.0;

float beltRockTransmittance(Frame* frame, float3 point)
{
    if (frame->scene.z < .5) return 1.0;
    float4 clip = mul(frame->belt_light_projection, float4(point, 1.0));
    float2 uv = clip.xy * .5 + .5;
    if (any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    float4 coverage = textures[TEX_BELT_LIGHT_MAP].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0);
    float3 n = frame->belt_normal.xyz;
    float3 q = point - frame->bodies[int(frame->scene.y)].xyz;
    float3 light = normalize(frame->sun.xyz - point);
    float along = dot(q, n) / (dot(light, n) + 1e-6);
    float c = clamp((along / frame->belt_light.w * .5 + .5) * BELT_MAP_SLICES, 0.0, BELT_MAP_SLICES);
    // Only slices wholly on the sun side count: the receiver's own slice holds
    // its own splat, which must not shadow it.
    float sum = 0.0;
    for (int slice = 0; slice < 4; ++slice)
        sum += coverage[slice] * (float(slice) >= c ? 1.0 : 0.0);
    return exp(-sum * BELT_MAP_OPACITY);
}

// Seen from outside the belt, each pixel's column through the thin dust is
// short and the belt would vanish; the dust density rises with the camera's
// distance from the slab (in slab half-heights) so the belt keeps reading as
// a faint disc. Both the near march and the baked far disc apply the same
// ramp, so handing over between them changes nothing but the integration.
static const float DUST_FAR_SCALE = 30.0;
static const float DUST_FAR_DISTANCE = 6.0; // half-heights at which the ramp saturates

float dustFarScale(float aboveSlab, float beyondEdge, float halfHeight, float brightness)
{
    float outside = max(max(aboveSlab, beyondEdge), 0.0) / (halfHeight * DUST_FAR_DISTANCE);
    return lerp(1.0, DUST_FAR_SCALE * brightness, saturate(outside));
}

// Disc-averaged brightness of a Lambertian sphere at phase angle alpha (the
// sun-rock-camera angle), relative to a flat lit surface: 2/3 at full phase,
// falling to zero at new phase. This is what a sub-pixel rock looks like.
float lambertSpherePhase(float alpha)
{
    return (sin(alpha) + (3.141592653589793 - alpha) * cos(alpha)) * (2.0 / (3.0 * 3.141592653589793));
}
