#include "common.slang"

// Belt dust: the particulate matter between the rocks, too fine to draw, as a
// scattering medium. The view ray is marched through the same analytic belt
// density the rock extinction uses (rings, gap, flared height), so the dust
// glows where the rocks are dense and thins at the edges; each sample takes
// sunlight through the belt, the planets' shadows and the rock transmittance
// map, and scatters it with a forward-peaked phase, so the belt lights up
// against the sun and stays a faint haze away from it. What lies behind is
// attenuated by the view transmittance (premultiplied output).
//
// Mode 0 marches at half resolution into textures[37], with the ray and the
// scene depth taken at the even full-resolution pixel of each block; mode 1
// composites it over the frame with a depth-aware 4-tap upsample, so the haze
// does not bleed across rock silhouettes. The ray is clipped to the belt slab,
// the outer edge and the scene depth; the start is jittered per pixel and
// frame, and the temporal pass settles the noise. Frame.quality.z switches
// the effect off.

static const uint MODE_MARCH = 0, MODE_COMPOSITE = 1;
static const int DUST_STEPS = 16;
static const int DUST_SUN_STEPS = 2;     // sun-ray density samples per view sample
static const float DUST_DENSITY = .08;   // dust extinction relative to the rock extinction coefficient: a full chord stays translucent
static const float DUST_ALBEDO = .5;     // single-scattering albedo of the medium
static const float3 DUST_COLOUR = float3(.62, .56, .48); // the belt's greyish-brown regolith, finer
static const float DUST_FORWARD = .55;   // Henyey-Greenstein g: fine dust scatters forward
static const float DUST_ISOTROPIC = .35; // share of an isotropic lobe, standing in for multiple scattering and back-scatter
// Seen from outside the belt, each pixel's column through the thin medium is
// short and the belt would vanish; the density rises with the camera's distance
// from the slab (in slab half-heights) so the belt keeps reading as a faint
// disc, the distant counterpart of the volumetric haze.
static const float DUST_FAR_SCALE = 30.0;
static const float DUST_FAR_DISTANCE = 6.0;
static const float DUST_SLAB = 3.5;      // slab half-height in flared scale heights; the Gaussian is negligible beyond
static const float UPSAMPLE_DEPTH_TOLERANCE = .05; // relative depth difference at which a tap loses half its weight

float phaseHG(float c, float g)
{
    float d = 1.0 + g * g - 2.0 * g * c;
    return (1.0 - g * g) / (4.0 * PI * d * sqrt(d));
}

float phase(float c)
{
    return lerp(phaseHG(c, DUST_FORWARD), 1.0 / (4.0 * PI), DUST_ISOTROPIC);
}

float sceneDepthAt(int2 pixel)
{
    float d = textures[18].Load(int3(pixel, 0)).r;
    return d >= .999999 ? FAR_Z : linearViewDepth(d);
}

// Sunlight left at a belt point through the dust itself: as beltExtinction
// but for the thin medium and with fewer samples, since the view march
// integrates over many of them.
float sunTransmittance(float3 q, float3 light, float4 ring, float3 n)
{
    float climb = max(abs(dot(light, n)), .08);
    float length = min((ring.w * .5 * 1.3 + abs(dot(q, n))) / climb, ring.y - ring.x);
    float ds = length / float(DUST_SUN_STEPS);
    float tau = 0.0;
    for (int i = 0; i < DUST_SUN_STEPS; ++i)
        tau += beltDensity(q + light * ((float(i) + .5) * ds), ring, n) * ds;
    return exp(-tau * ring.z * BELT_EXTINCTION * DUST_DENSITY);
}

float4 march(float2 fullResPixel, int2 depthPixel)
{
    Frame* frame = root.frame;
    float4 ring = frame->belt_ring;
    float3 rd = rayDirection(fullResPixel);
    float3 n = frame->belt_normal.xyz;
    float3 giant = frame->bodies[int(frame->scene.y)].xyz; // camera relative
    float3 q0 = -giant;                                     // camera in the giant's frame

    // Slab between the planes h = +-H.
    float halfHeight = ring.w * .5 * 1.4 * DUST_SLAB;
    float hn = dot(q0, n), dn = dot(rd, n);
    float t0, t1;
    if (abs(dn) < 1e-5)
    {
        if (abs(hn) > halfHeight) return float4(0.0, 0.0, 0.0, 0.0);
        t0 = 0.0;
        t1 = FAR_Z;
    }
    else
    {
        t0 = (-halfHeight - hn) / dn;
        t1 = (halfHeight - hn) / dn;
        if (t0 > t1) { float s = t0; t0 = t1; t1 = s; }
    }
    // Outer cylinder at the end of the density tail.
    float outer = ring.x + BELT_OUTER_TAIL * (ring.y - ring.x);
    float3 qp = q0 - n * hn, rp = rd - n * dn;
    float a = dot(rp, rp), b = 2.0 * dot(qp, rp), c = dot(qp, qp) - outer * outer;
    if (a > 1e-8)
    {
        float disc = b * b - 4.0 * a * c;
        if (disc < 0.0) return float4(0.0, 0.0, 0.0, 0.0);
        float s = sqrt(disc);
        t0 = max(t0, (-b - s) / (2.0 * a));
        t1 = min(t1, (-b + s) / (2.0 * a));
    }
    t0 = max(t0, 0.0);
    t1 = min(t1, sceneDepthAt(depthPixel) / max(dot(rd, frame->forward_exposure.xyz), 1e-4));
    if (t1 <= t0) return float4(0.0, 0.0, 0.0, 0.0);
    float outside = max(max(abs(hn) - halfHeight, 0.0), max(length(qp) - outer, 0.0)) / (halfHeight * DUST_FAR_DISTANCE);
    float farScale = lerp(1.0, DUST_FAR_SCALE * frame->dust.z, saturate(outside));

    // Tint and saturation from the panel: the tinted colour pulled toward or past its luminance.
    float3 tinted = DUST_COLOUR * frame->dust_tint.xyz;
    float3 dustColour = max(lerp(dot(tinted, float3(.2126, .7152, .0722)).xxx, tinted, frame->dust.w), 0.0);
    float jitter = hash(float3(fullResPixel, frac(frame->camera_time.w * 13.0) * 100.0 + frame->jitter.x * 37.0));
    float ds = (t1 - t0) / float(DUST_STEPS);
    float transmittance = 1.0;
    float3 radiance = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < DUST_STEPS; ++i)
    {
        float t = t0 + (float(i) + jitter) * ds;
        float3 p = rd * t;
        float3 q = p - giant;
        float density = beltDensity(q, ring, n) * ring.z * farScale;
        if (density < 1e-5) continue;
        float sigma = density * BELT_EXTINCTION * DUST_DENSITY * frame->dust.x; // extinction per unit length
        float3 light = normalize(frame->sun.xyz - p);
        float sunlight = frame->sun.w * sunTransmittance(q, light, ring, n) * solarVisibility(frame, p, -1)
                       * beltRockTransmittance(frame, p);
        float3 inscatter = dustColour * DUST_ALBEDO * frame->dust.y * sigma * phase(dot(light, rd)) * sunlight;
        // Analytic integral of the in-scatter over the step under constant density.
        float stepTransmittance = exp(-sigma * ds);
        radiance += transmittance * inscatter * (1.0 - stepTransmittance) / sigma;
        transmittance *= stepTransmittance;
        if (transmittance < .005) break;
    }
    return float4(radiance, 1.0 - transmittance);
}

// Depth-aware upsample: the four half-resolution taps around the pixel,
// bilinear weights scaled by how close each tap's scene depth is to this
// pixel's, so dust in front of a rock does not smear over its edge.
float4 composite(float2 position)
{
    int2 pixel = int2(position);
    float depth = sceneDepthAt(pixel);
    float2 halfSize = floor(root.frame->options.xy * .5);
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
            float4 dust = textures[37].Load(int3(tap, 0));
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
    if (frame->belt_ring.z <= 0.0 || frame->quality.z < .5) return float4(0.0, 0.0, 0.0, 0.0);
    if (root.mode == MODE_COMPOSITE) return composite(position.xy);
    int2 depthPixel = int2(position.xy) * 2;
    return march(float2(depthPixel) + .5, depthPixel);
}
