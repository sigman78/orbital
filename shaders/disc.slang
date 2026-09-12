#include "common.slang"

// The far belt: dust and rocks as one baked disc, blended in as the camera
// leaves the belt (Frame.belt_disc.y is the blend weight the near paths fade
// out by). Two maps over the belt plane, in the orthographic disc frame of
// belt_disc_projection: textures[38] carries the sunlight reaching the plane
// through the belt and the planets' shadows, baked here in mode 0 every few
// frames; textures[39] carries the rocks' coverage times albedo, splatted by
// discsplat.slang. Mode 1 draws the far tier: a short march through the
// slab's thickness, so a face-on view integrates one or two samples and an
// edge-on view keeps the belt's visible thickness, with the analytic density
// for the dust, the vertical profile applied to the rock coverage, and the
// baked sunlight for both. Output is premultiplied; the near march and the
// splats carry the complementary weight, so the crossfade never pops.

static const uint MODE_BAKE = 0, MODE_DRAW = 1;
static const int DISC_STEPS = 12;
static const float DUST_DENSITY = .08;   // as dust.slang: dust extinction relative to the rock coefficient
static const float DUST_ALBEDO = .5;
static const float3 DUST_COLOUR = float3(.62, .56, .48);
static const float DUST_FORWARD = .55, DUST_ISOTROPIC = .35;
static const float DUST_SLAB = 3.5;      // slab half-height in flared scale heights
static const float ROCK_AMBIENT = .004;  // the splats' ambient share, relative to the sun's irradiance
static const int MAX_FOOTPRINT = 4;      // box-filter taps per axis when the disc is minified

float phaseHG(float c, float g)
{
    float d = 1.0 + g * g - 2.0 * g * c;
    return (1.0 - g * g) / (4.0 * PI * d * sqrt(d));
}
float dustPhase(float c) { return lerp(phaseHG(c, DUST_FORWARD), 1.0 / (4.0 * PI), DUST_ISOTROPIC); }

float sceneDepthAt(int2 pixel)
{
    float d = textures[18].Load(int3(pixel, 0)).r;
    return d >= .999999 ? FAR_Z : linearViewDepth(d);
}

// Plane-space uv of a camera-relative point.
float2 discUv(Frame* frame, float3 p) { return mul(frame->belt_disc_projection, float4(p, 1.0)).xy * .5 + .5; }

// --- Mode 0: sunlight bake ------------------------------------------------------
float4 bake(float2 position)
{
    Frame* frame = root.frame;
    float texels = frame->belt_disc.x;
    float2 ndc = (position / texels) * 2.0 - 1.0;
    float3 giant = frame->bodies[int(frame->scene.y)].xyz;
    // Inverse of the orthographic projection: right along +x, up along -y (see write_light_projection).
    float3 point = giant + frame->belt_disc_right.xyz * (ndc.x * frame->belt_disc_right.w)
                 - frame->belt_disc_up.xyz * (ndc.y * frame->belt_disc_up.w);
    float sunlight = beltExtinction(frame, point) * solarVisibility(frame, point, -1);
    return float4(sunlight, 0.0, 0.0, 1.0);
}

// --- Mode 1: the far tier ---------------------------------------------------------
float4 rockMapAt(float2 uv, int taps)
{
    float texels = root.frame->belt_disc.z;
    if (taps <= 1) return textures[39].SampleLevel(samplers[0], uv, 0.0);
    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    float2 step = 1.0 / texels;
    for (int j = 0; j < taps; ++j)
        for (int i = 0; i < taps; ++i)
            sum += textures[39].SampleLevel(samplers[0], uv + (float2(i, j) - .5 * float(taps - 1)) * step, 0.0);
    return sum / float(taps * taps);
}

float4 draw(float2 position)
{
    Frame* frame = root.frame;
    float weight = frame->belt_disc.y;
    float4 ring = frame->belt_ring;
    float3 rd = rayDirection(position);
    float3 n = frame->belt_normal.xyz;
    float3 giant = frame->bodies[int(frame->scene.y)].xyz;
    float3 q0 = -giant;

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
    t1 = min(t1, sceneDepthAt(int2(position)) / max(dot(rd, frame->forward_exposure.xyz), 1e-4));
    if (t1 <= t0) return float4(0.0, 0.0, 0.0, 0.0);

    // Tint and saturation from the panel, as dust.slang.
    float3 tinted = DUST_COLOUR * frame->dust_tint.xyz;
    float3 dustColour = max(lerp(dot(tinted, float3(.2126, .7152, .0722)).xxx, tinted, frame->dust.w), 0.0);
    // The same distance ramp as the near march, so the handover is seamless.
    float farScale = dustFarScale(abs(hn) - halfHeight, length(qp) - outer, halfHeight, frame->dust.z);
    float dustDensity = frame->dust.x * farScale, dustBrightness = frame->dust.y;

    // Rock map minification: texels per pixel at the slab's distance.
    float texelSize = 2.0 * frame->belt_disc_right.w / frame->belt_disc.z;
    float pixelAngle = 2.0 * frame->right_tan.w / frame->options.y;
    int taps = clamp(int(round(t0 * pixelAngle / texelSize)), 3, MAX_FOOTPRINT); // at least 3x3: the map is a rock-count estimate and wants smoothing

    float jitter = hash(float3(position, frac(frame->camera_time.w * 11.0) * 100.0 + frame->jitter.y * 29.0));
    float ds = (t1 - t0) / float(DISC_STEPS);
    float transmittance = 1.0;
    float3 radiance = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < DISC_STEPS; ++i)
    {
        float t = t0 + (float(i) + jitter) * ds;
        float3 p = rd * t;
        float3 q = p - giant;
        float h = dot(q, n);
        float r = length(q - n * h);
        float radial = (r - ring.x) / (ring.y - ring.x);
        float scaleHeight = ring.w * .5 * lerp(.6, 1.4, saturate(radial));
        float vertical = exp(-h * h / (2.0 * scaleHeight * scaleHeight));
        float profile = vertical / (2.5066283 * scaleHeight); // normalised: the column integrates to one
        float density = beltDensity(q, ring, n) * ring.z;
        float2 uv = discUv(frame, p);
        float sunlight = frame->sun.w * textures[38].SampleLevel(samplers[0], uv, 0.0).r;
        float3 light = normalize(frame->sun.xyz - p);

        float sigmaDust = density * BELT_EXTINCTION * DUST_DENSITY * dustDensity;
        float3 dust = dustColour * DUST_ALBEDO * dustBrightness * sigmaDust * dustPhase(dot(light, rd)) * sunlight;

        float4 rocks = rockMapAt(uv, taps) * profile; // coverage per unit length along the ray
        float phase = acos(clamp(dot(light, -rd), -1.0, 1.0));
        float3 rock = rocks.rgb * (sunlight / PI * lambertSpherePhase(phase) + frame->sun.w * ROCK_AMBIENT);

        float sigma = sigmaDust + rocks.a;
        if (sigma < 1e-6) continue;
        float stepTransmittance = exp(-sigma * ds);
        radiance += transmittance * (dust + rock) * (1.0 - stepTransmittance) / sigma;
        transmittance *= stepTransmittance;
        if (transmittance < .005) break;
    }
    return float4(radiance, 1.0 - transmittance) * weight;
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    if (root.mode == MODE_BAKE) return bake(position.xy);
    if (root.frame->belt_ring.z <= 0.0 || root.frame->belt_disc.y <= 0.0) return float4(0.0, 0.0, 0.0, 0.0);
    return draw(position.xy);
}
