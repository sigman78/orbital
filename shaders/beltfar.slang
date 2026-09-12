// The belt dust and the far-belt disc share these: the medium's constants,
// the sunlight bake over the belt plane, and the far tier's march through
// the slab reading the baked maps. Included by dust.slang (which composites
// the near march and the far tier in one blend) and disc.slang (the bake).
//
// The far tier: dust and rocks as one baked disc. Two maps over the belt
// plane, in the orthographic disc frame of belt_disc_projection: textures[38]
// carries the sunlight reaching the plane through the belt and the planets'
// shadows; textures[39] the small rocks' coverage times albedo, splatted by
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
static const int MAX_FOOTPRINT = 4;      // box-filter taps per axis when the rock map is minified

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

// The dust colour after the panel's tint and saturation.
float3 dustColour(Frame* frame)
{
    float3 tinted = DUST_COLOUR * frame->dust_tint.xyz;
    return max(lerp(dot(tinted, float3(.2126, .7152, .0722)).xxx, tinted, frame->dust.w), 0.0);
}

// The view ray's segment inside the belt slab and outer cylinder, clipped to
// the scene; false when it misses. Also returns the camera's excess height
// above the slab and distance beyond the edge, for the distance ramp.
bool beltSegment(Frame* frame, float3 rd, int2 depthPixel, out float t0, out float t1, out float aboveSlab, out float beyondEdge, out float halfHeight)
{
    float4 ring = frame->belt_ring;
    float3 n = frame->belt_normal.xyz;
    float3 q0 = -frame->bodies[int(frame->scene.y)].xyz; // the camera in the giant's frame
    halfHeight = ring.w * .5 * 1.4 * DUST_SLAB;
    float hn = dot(q0, n), dn = dot(rd, n);
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
    float outer = ring.x + BELT_OUTER_TAIL * (ring.y - ring.x);
    float3 qp = q0 - n * hn, rp = rd - n * dn;
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
    t1 = min(t1, sceneDepthAt(depthPixel) / max(dot(rd, frame->forward_exposure.xyz), 1e-4));
    aboveSlab = abs(hn) - halfHeight;
    beyondEdge = length(qp) - outer;
    return t1 > t0;
}

// Plane-space uv of a camera-relative point.
float2 discUv(Frame* frame, float3 p) { return mul(frame->belt_disc_projection, float4(p, 1.0)).xy * .5 + .5; }

// --- Sunlight bake over the belt plane -------------------------------------------
// R: transmittance through the dust's own thin medium (what the near march
// shades the dust with), G: through the belt's rock extinction (what the
// splats are shaded with), B: the planets' shadows.
float4 bakeSunlight(float2 position)
{
    Frame* frame = root.frame;
    float texels = frame->belt_disc.x;
    float2 ndc = (position / texels) * 2.0 - 1.0;
    float3 giant = frame->bodies[int(frame->scene.y)].xyz;
    // Inverse of the orthographic projection: right along +x, up along -y (see write_light_projection).
    float3 point = giant + frame->belt_disc_right.xyz * (ndc.x * frame->belt_disc_right.w)
                 - frame->belt_disc_up.xyz * (ndc.y * frame->belt_disc_up.w);
    float rockTransmittance = beltExtinction(frame, point);
    float dustTransmittance = pow(rockTransmittance, DUST_DENSITY); // the same optical depth, scaled to the dust
    return float4(dustTransmittance, rockTransmittance, solarVisibility(frame, point, -1), 1.0);
}

// --- The far tier -------------------------------------------------------------------
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

// Premultiplied radiance and opacity of the far belt along this pixel's ray, unweighted.
float4 farBelt(float2 position)
{
    Frame* frame = root.frame;
    float4 ring = frame->belt_ring;
    float3 rd = rayDirection(position);
    float3 n = frame->belt_normal.xyz;
    float3 giant = frame->bodies[int(frame->scene.y)].xyz;
    float t0, t1, aboveSlab, beyondEdge, halfHeight;
    if (!beltSegment(frame, rd, int2(position), t0, t1, aboveSlab, beyondEdge, halfHeight))
        return float4(0.0, 0.0, 0.0, 0.0);

    // The same distance ramp as the near march, so the handover is seamless.
    float farScale = dustFarScale(aboveSlab, beyondEdge, halfHeight, frame->dust.z);
    float3 colour = dustColour(frame);
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
        float3 baked = textures[38].SampleLevel(samplers[0], uv, 0.0).rgb;
        float dustSunlight = frame->sun.w * baked.r * baked.b, rockSunlight = frame->sun.w * baked.g * baked.b;
        float3 light = normalize(frame->sun.xyz - p);

        float sigmaDust = density * BELT_EXTINCTION * DUST_DENSITY * dustDensity;
        float3 dust = colour * DUST_ALBEDO * dustBrightness * sigmaDust * dustPhase(dot(light, rd)) * dustSunlight;

        float4 rocks = rockMapAt(uv, taps) * profile; // coverage per unit length along the ray
        float phase = acos(clamp(dot(light, -rd), -1.0, 1.0));
        float3 rock = rocks.rgb * (rockSunlight / PI * lambertSpherePhase(phase) + frame->sun.w * ROCK_AMBIENT);

        float sigma = sigmaDust + rocks.a;
        if (sigma < 1e-6) continue;
        float stepTransmittance = exp(-sigma * ds);
        radiance += transmittance * (dust + rock) * (1.0 - stepTransmittance) / sigma;
        transmittance *= stepTransmittance;
        if (transmittance < .005) break;
    }
    return float4(radiance, 1.0 - transmittance);
}
