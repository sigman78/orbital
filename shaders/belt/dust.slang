#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../scene/view.slang"
#include "../lib/noise.slang"
#include "beltfar.slang"
#include "rockclass.slang"

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
// layers would. The ray start is dithered per pixel with interleaved gradient
// noise, rotated per frame while the temporal pass is on to settle it (by the
// frame count, so a paused simulation keeps decorrelating it) and still when
// it is off, and the
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

// --- Analytic sub-pixel rocks ---------------------------------------------------------
// Rocks projecting under the point cut-off are not drawn (the cull fades the splats
// out over one cut-off width). With Frame.speckle.x set the march stands in for them:
// a point field on four planes through the slab, cells in each band's co-rotating
// frame holding rocks drawn from the population's size law, lit as the cull lights a
// splat and deposited with the march pixel's footprint. Anchored in the belt, so it
// parallaxes and orbits like the rocks it replaces, and with their energy (the same
// covered area and shading), so the cut-off can move without the belt's brightness.
static const int SPECKLE_LAYERS = 3;
static const float SPECKLE_HEIGHTS[SPECKLE_LAYERS] = { -1.0, 0.0, 1.0 }; // in half thicknesses
static const float SPECKLE_CELL = .05;  // base cell, world units; octaves double it, chosen by the pixel footprint
static const float SPECKLE_SIGMA = .5;  // the deposit's footprint, march pixels
static const float SPECKLE_DENSE = 1.0; // rocks per cell above which the cell is its mean with shot noise
static const float SPECKLE_MAX_FOOTPRINT = 8.0; // plane units a march pixel may span before the field gives up (grazing)
static const float SPLAT_RIM = .12;     // as cull.slang
static const float ROCK_SIZE_MIN = .5, ROCK_SIZE_MAX = 12.0, ROCK_RADIUS_SCALE = .025; // mirrors geometry.cpp's population and belt::rock_radius_scale
static const float ROCK_SIZE_RATIO = 1.0 / 576.0; // (max / min)^(1 - 3), the size law's inverse-transform constant
static const float ROCK_AXIS_SQUARE = 1.0208;     // E[(.75 + .5 u)^2], the generator's per-axis scale jitter

struct SpeckleView
{
    float3 e1, e2;      // the belt plane's axes in the giant's frame
    float kFull;        // full-resolution pixels per unit at unit depth
    float3 light;
    float rockPhase;    // Lambert sphere phase for the segment
    float rim;          // the splats' forward-scatter rim at this phase
    float cutoff;       // point cut-off, full-resolution pixels
    float rocksPerArea; // rocks per unit plane area at ring density 1
    float spinTime;
    float shading;      // radiance per unit albedo at the ray's crossing of the belt plane: the layers are close, it serves all
};

uint speckleMix(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
uint speckleHash(int2 cell, int layer, int octave)
{
    return speckleMix(uint(cell.x) * 73856093u ^ uint(cell.y) * 19349663u ^ uint(layer * 64 + octave + 32) * 83492791u);
}
float speckleUnit(uint bits) { return float(bits & 0xffffffu) / 16777216.0; }

// The scanned rock sets' mean linear albedo, measured (rock and boulder averaged for the
// silicate and metallic classes, rock face for the carbonaceous): what the cull's coarsest-mip
// sample of them gives a splat. The bake's ROCK_SET_AVERAGE constants sit above these.
static const float3 ROCK_SET_MEAN_S = float3(.375, .32, .255);
static const float3 ROCK_SET_MEAN_C = float3(.207, .154, .112);

// A rock's mean albedo as the cull takes it for a splat, from 32 random bits: the class by the
// radial gradient (rockClass), its colour and variation (rockColour), the set's mean and the
// material seed's variation.
float3 speckleAlbedo(uint r, float radial)
{
    float carbon = lerp(.12, .85, smoothstep(0.0, 1.0, radial));
    float h = float(r & 0xffffu) / 65536.0;
    uint composition = h < METAL_FRACTION ? CLASS_M : h < METAL_FRACTION + carbon * (1.0 - METAL_FRACTION) ? CLASS_C : CLASS_S;
    float3 base = composition == CLASS_S ? float3(.80, .76, .70) : composition == CLASS_C ? float3(.36, .36, .36) : float3(.70, .71, .74);
    float variation = .8 + .4 * float((r >> 16) & 0xffu) / 256.0;
    float material = .8 + .4 * float(r >> 24) / 256.0;
    return base * (composition == CLASS_C ? ROCK_SET_MEAN_C : ROCK_SET_MEAN_S) * (variation * material);
}

// A point's Gaussian deposit onto this pixel from a screen offset in march pixels, integrating to one.
float speckleKernel(float2 offset, float sigma)
{
    return exp(-dot(offset, offset) / (2.0 * sigma * sigma)) / (2.0 * PI * sigma * sigma);
}

// The sub-cut-off rocks around this pixel's crossing of one layer: their radiance
// deposited on the pixel, and the fraction of it they cover (they hide what is
// behind them, as the splats do; a rock in front of the planet is dark against it).
float4 speckleLayer(BeltParams belt, ViewParams view, SpeckleView sv, float3 p, int layer)
{
    float3 q = p - belt.centre;
    float2 plane = float2(dot(q, sv.e1), dot(q, sv.e2));
    float r = length(plane);
    float t = (r - belt.inner) / (belt.outer - belt.inner);
    float ring = beltRingDensity(t);
    if (ring < 1e-4) return float4(0.0, 0.0, 0.0, 0.0);
    // The layers share the column's rocks by the height profile at this radius.
    float scaleHeight = belt.thickness * .5 * lerp(.6, 1.4, saturate(t));
    float h = dot(q, belt.normal);
    float weightSum = 0.0;
    for (int j = 0; j < SPECKLE_LAYERS; ++j)
    {
        float hj = SPECKLE_HEIGHTS[j] * belt.thickness * .5;
        weightSum += exp(-hj * hj / (2.0 * scaleHeight * scaleHeight));
    }
    float layerWeight = exp(-h * h / (2.0 * scaleHeight * scaleHeight)) / weightSum;
    // The rocks under twice the cut-off at this depth (where the splats' fade ends): their
    // share of the population, their mean squared size, and so their number per unit area.
    float z = max(dot(p, view.forward), .1);
    float pixelsPerUnit = sv.kFull / z;
    float sMax = min(2.0 * sv.cutoff / (pixelsPerUnit * ROCK_RADIUS_SCALE), ROCK_SIZE_MAX);
    if (sMax <= ROCK_SIZE_MIN) return float4(0.0, 0.0, 0.0, 0.0);
    float ratio = (ROCK_SIZE_MIN / sMax) * (ROCK_SIZE_MIN / sMax);
    float fraction = (1.0 - ratio) / (1.0 - ROCK_SIZE_RATIO);
    // Their mean squared size, the splats' fade-in over the last octave folded in (a size law
    // of s^-3 spends equal energy per octave; the faded octave keeps 2 ln 2 - 1 of it).
    float sCut = sMax * .5;
    float meanSquare = (sCut < ROCK_SIZE_MAX ? log(sCut / ROCK_SIZE_MIN) + .3863 : log(sMax / ROCK_SIZE_MIN))
                     * 2.0 * ROCK_SIZE_MIN * ROCK_SIZE_MIN / (1.0 - ratio);
    float density = sv.rocksPerArea * ring * layerWeight * fraction;
    // The cells orbit with the band, as the rocks they stand for do.
    int band = clamp(int(saturate(t) * float(ORBITAL_BELT_BANDS)), 0, ORBITAL_BELT_BANDS - 1);
    float bandRadius = belt.inner + (float(band) + .5) / float(ORBITAL_BELT_BANDS) * (belt.outer - belt.inner);
    float angle = BELT_SPIN_RATE * pow((belt.inner + belt.outer) * .5 / bandRadius, BELT_SHEAR) * sv.spinTime;
    float ca = cos(angle), sa = sin(angle);
    float2 rotated = float2(plane.x * ca + plane.y * sa, plane.y * ca - plane.x * sa);
    float3 wx = sv.e1 * ca + sv.e2 * sa, wy = sv.e2 * ca - sv.e1 * sa; // the co-rotating axes in the giant's frame
    // Screen offset per unit of plane offset at the crossing, in march pixels, with the
    // perspective term (an offset along the view moves a point off the axis on screen).
    float2 perp = float2(dot(p, view.right), dot(p, view.up)) / z;
    float2 sx = (float2(dot(wx, view.right), dot(wx, view.up)) - perp * dot(wx, view.forward)) * (.5 * pixelsPerUnit);
    float2 sy = (float2(dot(wy, view.right), dot(wy, view.up)) - perp * dot(wy, view.forward)) * (.5 * pixelsPerUnit);
    // A march pixel's footprint on the plane is an ellipse, long where the plane is
    // foreshortened; its long axis sets the cell scale, so the cells around the crossing
    // cover the deposit's reach on screen along both axes.
    float a = dot(sx, sx), c = dot(sy, sy), b = dot(sx, sy);
    float eigenMin = .5 * (a + c) - sqrt(.25 * (a - c) * (a - c) + b * b);
    if (eigenMin < 1e-8) return float4(0.0, 0.0, 0.0, 0.0); // edge-on
    float footprint = min(rsqrt(eigenMin), SPECKLE_MAX_FOOTPRINT);
    float shading = sv.shading;
    float areaScale = SPLAT_AREA * PI * .25 * pixelsPerUnit * pixelsPerUnit; // covered march pixels per unit radius squared
    // Cells between one and four footprints wide; two octaves blended, so a moving camera
    // never sees the pattern switch scale.
    float scaled = log2(footprint / SPECKLE_CELL) + 1.0;
    int lower = int(floor(scaled));
    float f = smoothstep(.15, .85, scaled - float(lower)); // one octave most of the way, both across the switch
    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    for (int o = 0; o < 2; ++o)
    {
        int octave = lower + o;
        float weight = o == 0 ? 1.0 - f : f;
        float cellSize = SPECKLE_CELL * exp2(float(octave));
        float mu = density * cellSize * cellSize;
        if (weight < 1e-3 || mu < 1e-4) continue;
        int2 base = int2(floor(rotated / cellSize));
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i)
            {
                int2 cell = base + int2(i, j);
                // Skip a cell no rock of which can reach this pixel: its nearest point on screen.
                float2 nearest = clamp(rotated / cellSize, float2(cell), float2(cell) + 1.0) * cellSize - rotated;
                float2 nearestScreen = sx * nearest.x + sy * nearest.y;
                if (dot(nearestScreen, nearestScreen) > 2.5 * 2.5 * SPECKLE_SIGMA * SPECKLE_SIGMA * 4.0) continue;
                uint hsh = speckleHash(cell, layer, octave);
                if (mu > SPECKLE_DENSE)
                {
                    // Many rocks: their mean with Poisson shot noise, spread over the cell as
                    // it lies on screen (a Gaussian with the projected cell's covariance).
                    float2 delta = (float2(cell) + .5) * cellSize - rotated;
                    float2 ex = sx * cellSize, ey = sy * cellSize; // the cell's edges on screen
                    float s2 = SPECKLE_SIGMA * SPECKLE_SIGMA;
                    float cxx = s2 + (ex.x * ex.x + ey.x * ey.x) / 12.0, cyy = s2 + (ex.y * ex.y + ey.y * ey.y) / 12.0;
                    float cxy = (ex.x * ex.y + ey.x * ey.y) / 12.0;
                    float det = cxx * cyy - cxy * cxy;
                    float2 o = sx * delta.x + sy * delta.y;
                    float kernel = exp(-.5 * (cyy * o.x * o.x - 2.0 * cxy * o.x * o.y + cxx * o.y * o.y) / det) / (2.0 * PI * sqrt(det));
                    float count = mu * (1.0 + (speckleUnit(hsh) - .5) * 3.4641 * rsqrt(mu));
                    float area = areaScale * ROCK_RADIUS_SCALE * ROCK_RADIUS_SCALE * meanSquare * ROCK_AXIS_SQUARE;
                    float deposit = weight * count * area * kernel;
                    sum += float4(speckleAlbedo(speckleMix(hsh ^ 0x68bc21ebu), saturate(t)) * deposit * shading, deposit);
                    continue;
                }
                int count = int(floor(mu + speckleUnit(hsh)));
                for (int k = 0; k < count; ++k)
                {
                    // Three words of random bits per rock: its place in the cell, its size, its albedo.
                    uint r0 = speckleMix(hsh + (uint(k) + 1u) * 0x9e3779b9u), r1 = speckleMix(r0 ^ 0x68bc21ebu), r2 = speckleMix(r1 + 0x2545f491u);
                    float2 delta = (float2(cell) + float2(r0 & 0xffffu, r0 >> 16) / 65536.0) * cellSize - rotated;
                    float u = speckleUnit(r1);
                    float size = ROCK_SIZE_MIN * rsqrt(1.0 - u * fraction * (1.0 - ROCK_SIZE_RATIO)); // the size law under sMax
                    float radius = size * ROCK_RADIUS_SCALE * (.75 + .5 * float(r1 >> 24) / 256.0);
                    float pixelRadius = radius * pixelsPerUnit;
                    float fade = 1.0 - saturate((pixelRadius - sv.cutoff) / sv.cutoff); // the splat's fade, mirrored
                    // A disc splat's soft edge, a pixel wide, loses part of a small disc's area
                    // (0.77 at half a pixel, 0.93 at two); the field stands in for it as drawn.
                    if (pixelRadius >= .5) fade *= 1.0 - .14 * rsqrt(pixelRadius * sqrt(SPLAT_AREA));
                    float deposit = weight * fade * areaScale * radius * radius * speckleKernel(sx * delta.x + sy * delta.y, SPECKLE_SIGMA);
                    sum += float4(speckleAlbedo(r2, saturate(t)) * deposit * shading, deposit);
                }
            }
    }
    return sum;
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

    // The march's own pixel, not the full-resolution one: the pattern is made for a unit
    // stride, and sampled every other pixel it folds into waves across the halo.
    float jitter = marchJitter(float2(depthPixel / 2), view.frame, temporalParams(frame).enabled);
    // Once per pixel: the light direction and the phase (the sun is far), and which bodies'
    // shadows the segment can enter at all.
    float3 light = normalize(sunLight.position - rd * (.5 * (t0 + t1)));
    float phase = dustPhase(dot(light, rd));
    uint shadows = shadowCandidates(frame, rd, t0, t1);
    // The analytic sub-pixel rocks: where the ray crosses each of their layers, taken as
    // the march passes it, behind what the march has accumulated so far.
    bool speckle = frame->speckle.x > 0.0;
    SpeckleView sv;
    float layerT[SPECKLE_LAYERS];
    if (speckle)
    {
        sv.e1 = normalize(cross(belt.normal, float3(0.0, 0.0, 1.0)));
        sv.e2 = cross(sv.e1, belt.normal);
        sv.kFull = view.size.y / (2.0 * view.tanHalfFov);
        sv.light = light;
        float cosPhase = clamp(dot(light, -rd), -1.0, 1.0);
        sv.rockPhase = lambertSpherePhase(acos(cosPhase));
        sv.rim = SPLAT_RIM * (1.0 - cosPhase) * .5;
        sv.cutoff = frame->speckle.x;
        sv.rocksPerArea = frame->speckle.y;
        sv.spinTime = view.time;
        float hn = dot(-belt.centre, belt.normal), dn = dot(rd, belt.normal);
        for (int k = 0; k < SPECKLE_LAYERS; ++k)
            layerT[k] = abs(dn) < 1e-5 ? -1.0 : (SPECKLE_HEIGHTS[k] * belt.thickness * .5 - hn) / dn;
        // Lit as the cull lights a splat, at the ray's crossing of the belt plane (clamped to
        // the segment): the rock extinction, the planets' shadows and the transmittance map.
        float3 crossing = rd * clamp(abs(dn) < 1e-5 ? t0 : -hn / dn, t0, t1);
        float shadowing = solarVisibilityMasked(frame, crossing, shadows) * beltExtinction(frame, crossing) * beltRockTransmittance(frame, crossing);
        sv.shading = sunLight.radiance / PI * shadowing * (sv.rockPhase + sv.rim) + sunLight.radiance * ROCK_AMBIENT;
    }
    float ds = (t1 - t0) / float(DUST_STEPS);
    float transmittance = 1.0;
    float3 radiance = float3(0.0, 0.0, 0.0);
    float4 speckleSum = float4(0.0, 0.0, 0.0, 0.0); // the field's radiance and covered fraction
    float tPrev = t0;
    for (int i = 0; i < DUST_STEPS; ++i)
    {
        float t = t0 + (float(i) + jitter) * ds;
        float3 p = rd * t;
        if (speckle)
            for (int k = 0; k < SPECKLE_LAYERS; ++k)
                if (layerT[k] > tPrev && layerT[k] <= t)
                    speckleSum += speckleLayer(belt, view, sv, rd * layerT[k], k);
        tPrev = t;
        float3 q = p - belt.centre;
        float density = beltDensity(q, belt) * belt.density * farScale;
        if (density < 1e-5) continue;
        float sigma = density * BELT_EXTINCTION * DUST_DENSITY * dustSettings.density; // extinction per unit length
        float sunlight = sunLight.radiance * sunTransmittance(q, light, belt) * solarVisibilityMasked(frame, p, shadows)
                       * beltRockTransmittance(frame, p);
        float3 inscatter = colour * DUST_ALBEDO * dustSettings.brightness * sigma * phase * sunlight;
        // Analytic integral of the in-scatter over the step under constant density.
        float stepTransmittance = exp(-sigma * ds);
        radiance += transmittance * inscatter * (1.0 - stepTransmittance) / sigma;
        transmittance *= stepTransmittance;
        if (transmittance < .005) break;
    }
    if (speckle)
    {
        for (int k = 0; k < SPECKLE_LAYERS; ++k)
            if (layerT[k] > tPrev && layerT[k] <= t1)
                speckleSum += speckleLayer(belt, view, sv, rd * layerT[k], k);
        // Behind the whole column, as the splats are (the dust is composited over them),
        // covering their share of what is behind and, blended in no order, of each other:
        // a covered area A in random layers passes (1 - exp(-A)) / A of its light.
        float covered = speckleSum.a;
        float passed = covered > 1e-4 ? (1.0 - exp(-covered)) / covered : 1.0;
        radiance += speckleSum.rgb * transmittance * passed;
        transmittance *= exp(-covered);
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

// The twinkle veil at this pixel: its layers' crossings inside the belt segment (so the
// scene occludes it), lit by the far-belt bake, behind the dust's transmittance.
float3 veilAt(float2 position, float transmittance)
{
    Frame* frame = root.frame;
    VeilParams veil = veilParams(frame);
    if (!veil.enabled || transmittance < .02) return float3(0.0, 0.0, 0.0);
    BeltParams belt = beltParams(frame);
    ViewParams view = viewParams(frame);
    LightParams sunLight = lightParams(frame);
    float3 rd = rayDirection(frame, position);
    float t0, t1, aboveSlab, beyondEdge, halfHeight;
    if (!beltSegment(frame, rd, int2(position), t0, t1, aboveSlab, beyondEdge, halfHeight))
        return float3(0.0, 0.0, 0.0);
    float3 light = normalize(sunLight.position - rd * (.5 * (t0 + t1)));
    float crossings[VEIL_LAYERS];
    veilCrossings(belt, rd, crossings);
    float3 sum = float3(0.0, 0.0, 0.0);
    for (int k = 0; k < VEIL_LAYERS; ++k)
    {
        if (crossings[k] < t0 || crossings[k] > t1) continue;
        float3 crossing = rd * crossings[k];
        float3 baked = textures[TEX_BELT_DISC_LIGHT].SampleLevel(samplers[SAMPLER_CLAMP], discUv(frame, crossing), 0.0).rgb;
        sum += veilLayer(frame, belt, view, veil, crossing, k, rd, light, sunLight.radiance / PI * baked.g * baked.b);
    }
    return sum * transmittance;
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
    // One blend: the near march and the far disc mixed by the LOD weight, the veil behind their dust.
    float4 near = dust.enabled && weight < 1.0 ? upsampleNear(position.xy) : float4(0.0, 0.0, 0.0, 0.0);
    float4 far = weight > 0.0 ? farBelt(position.xy) : float4(0.0, 0.0, 0.0, 0.0);
    float4 blended = near * (1.0 - weight) + far * weight;
    return blended + float4(veilAt(position.xy, 1.0 - blended.a), 0.0);
}
