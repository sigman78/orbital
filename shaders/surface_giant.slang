// The gas giant: the cloud deck advected along a baked wind map, baked
// cloud-top relief, a two-deck parallax, the polar cyclone caps, night-side
// lightning, and a scattering light model in place of the surface BRDF.
#include "surface_common.slang"

static const float2 GAS_TEXELS = float2(3600.0, 1800.0);
static const float2 GAS_DETAIL_TEXELS = float2(4096.0, 2048.0);
static const uint GAS_FLOW = 40, GAS_DETAIL = 41, GAS_RELIEF = 42, GAS_POLAR = 43, GAS_POLAR_FLOW = 44;
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
    float2 flow = (textures[GAS_POLAR_FLOW].SampleLevel(samplers[0], atlas, 0.0).rg * 2.0 - 1.0) * GAS_POLAR_FLOW_UNIT * giant.x;
    float cycle = giant.y;
    float phase = fbm2(uv * float2(40, 40) + south * 7.0, 2);
    float p0 = frac(t / cycle + phase), p1 = frac(p0 + .5);
    float w0 = 1.0 - abs(2.0 * p0 - 1.0);
    float2 d0 = -flow * (p0 - .5) * cycle, d1 = -flow * (p1 - .5) * cycle;
    float3 c0 = textures[GAS_POLAR].SampleGrad(samplers[0], atlas + d0 * float2(1.0, .5), dx, dy).rgb;
    float3 c1 = textures[GAS_POLAR].SampleGrad(samplers[0], atlas + d1 * float2(1.0, .5), dx, dy).rgb;
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
    float filaments = (sampleSphere(textures[GAS_DETAIL], uv).r - .5) * 6.0;
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
float upperDeck(float2 uv) { return smoothstep(.3, .7, sampleSphere(textures[GAS_RELIEF], uv).a); }

float3 layeredDeck(float2 uv, float2 lift, float2 cast, float2 layers, float t, float fine, float extreme)
{
    float3 lower = sampleSphere(textures[4], uv).rgb * giantDetail(uv, t, fine, extreme);
    if (layers.x <= 0.0) return lower;
    float3 upper = sampleSphere(textures[4], uv + lift).rgb * giantDetail(uv + lift, t, fine, extreme);
    float upperHere = upperDeck(uv + lift);
    float shade = 1.0 - layers.y * upperDeck(uv + cast) * (1.0 - upperHere);
    return lerp(lower * shade, upper, upperHere);
}

Material giantMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float3 V, float visibility)
{
    float4 giant = root.frame->giant;
    if (giant.w < .5) giant.x = 0.0; // the panel's flow switch off: the deck and the caps hold still
    float t = root.frame->camera_time.w;
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float4 flowMap = sampleSphere(textures[GAS_FLOW], uv);
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
    float2 layers = root.frame->giant_layers.xy;
    float3 viewTS = float3(dot(V, T), dot(V, B), max(dot(V, N), .15));
    float3 lightTS = float3(dot(L, T), dot(L, B), max(dot(L, N), .2));
    float2 lift = viewTS.xy / viewTS.z * layers.x;  // toward the viewer, in texture units
    float2 cast = lightTS.xy / lightTS.z * layers.x; // toward the sun
    // Close-range streaks: 1 once a filament texel covers 8 pixels; nothing with the panel's switch off.
    float streaks = root.frame->giant_layers.z;
    float extreme = streaks > 0.0 ? magnification(uv, GAS_DETAIL_TEXELS * 4.0) : 0.0;
    float3 c0 = layeredDeck(uv0, lift, cast, layers, t, fine, extreme);
    float3 c1 = layeredDeck(uv1, lift, cast, layers, t, fine, extreme);
    Material m;
    m.albedo = lerp(c1, c0, w0);
    if (extreme > 0.0) m.albedo *= giantCloseDetail(lerp(uv1, uv0, w0), along, fine * streaks, extreme);
    // Over the outer part of the cap's radius the map gives way to the baked polar caps.
    float capSin = GAS_CAP_SIN * max(root.frame->giant_night.z, 1e-3);
    float capEdge = length(localN.xz) / capSin; // 1 at the cap's edge
    float polar = root.frame->giant_night.z > 0.0
                      ? (1.0 - smoothstep(1.0 - root.frame->giant_night.w, 1.0, capEdge)) * root.frame->giant_more.w : 0.0;
    float2 capUV = .5 + .5 * localN.xz / capSin; // and its footprint, taken outside the branch
    float2 capDx = ddx(capUV) * float2(1.0, .5), capDy = ddy(capUV) * float2(1.0, .5);
    if (polar > 0.0) m.albedo = lerp(m.albedo, giantPolar(localN, capUV, capDx, capDy, t, giant), polar);
    m.rough = .95;
    m.water = 0.0;
    m.visibility = visibility * beltShadow(input.worldPosition);
    // The panel exaggerates the physical slopes so the relief reads along the
    // terminator; under a high sun the same tilt would emboss the whole disc,
    // so it fades to a fraction there, where the true slopes are invisible anyway.
    float grazing = 1.0 - smoothstep(.05, .5, dot(N, L));
    float strength = root.frame->giant_more.z * lerp(.15, 1.0, grazing) * (1.0 - polar);
    float2 encoded = lerp(sampleSphere(textures[GAS_RELIEF], uv1).xy, sampleSphere(textures[GAS_RELIEF], uv0).xy, w0) * 2.0 - 1.0;
    float2 slope = encoded * abs(encoded) * GAS_SLOPE_MAX * strength;
    m.N = normalize(N - T * slope.x - B * slope.y);
    return m;
}

// --- Night-side lightning -----------------------------------------------------------
//
// Galileo imaged 26 storms on the night side, up to 1500 km across, with the
// flashes themselves bell-shaped spots of 45 to 80 km half-width, concentrated
// near 50 degrees in both hemispheres (more in the north) in regions of
// cyclonic shear; Juno measured peak rates of four flashes a second and none
// near the equator. Here storm cells are picked by hash on a grid over the
// map and live for a couple of minutes each; every storm flashes at random
// moments, each flash a fast rise and a 120 ms decay, a small hot core with a
// wider glow of cloud lit from below. Where the core is smaller than a pixel
// it widens to one and dims to keep its energy, so a flash stays a spark for
// the bloom from any distance.

static const float2 STORM_CELLS = float2(512.0, 256.0);
static const float STORM_EPOCH = 150.0;       // seconds a set of storms lasts
static const float STORM_DENSITY = 4e-4;      // fraction of cells that hold a storm
static const float FLASH_SLOT = .125;         // seconds; at most one flash per storm per slot
static const float FLASH_DECAY = .12;         // seconds
static const float FLASH_CORE = 60.0 / 71492.0 / (2.0 * PI);   // sigma of the core in texture units at the equator
static const float FLASH_GLOW = 500.0 / 71492.0 / (2.0 * PI);  // sigma of the lit cloud around it

float hashCell(float2 cell, float salt) { return hash2(cell * 1.37 + salt * float2(17.13, 5.71)); }

// Where lightning happens: the 50 degree bands, the north more, some over the
// poles, nothing at the equator; and the shear regions the flow map marks.
float stormWeight(float lat, float shear)
{
    float bands = exp(-pow((abs(lat) - 50.0) / 11.0, 2.0)) * (lat > 0.0 ? 1.0 : .7);
    float polar = smoothstep(60.0, 75.0, abs(lat)) * .5;
    return saturate(bands + polar) * (.4 + .6 * shear);
}

float3 giantLightning(float2 uv, float t, float shear, float2 footprint)
{
    float4 night = root.frame->giant_night;
    if (night.x <= 0.0 || night.y <= 0.0) return float3(0.0, 0.0, 0.0);
    float lat = (.5 - uv.y) * 180.0;
    float weight = stormWeight(lat, shear);
    if (weight < .01) return float3(0.0, 0.0, 0.0);
    float epoch = floor(t / STORM_EPOCH);
    float epochPhase = frac(t / STORM_EPOCH);
    float life = smoothstep(0.0, .07, epochPhase) * (1.0 - smoothstep(.93, 1.0, epochPhase));
    // Flash probability per storm per slot that gives the panel's rate over the planet's storms.
    float storms = STORM_CELLS.x * STORM_CELLS.y * STORM_DENSITY * .45; // .45: the mean of the weight over the map
    float chance = night.x * FLASH_SLOT / storms;
    float cosLat = max(cos(radians(lat)), .05);
    float2 cellSize = 1.0 / STORM_CELLS;
    float2 base = floor(uv * STORM_CELLS);
    float total = 0.0;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
    {
        float2 cell = base + float2(dx, dy);
        cell.x -= STORM_CELLS.x * floor(cell.x / STORM_CELLS.x); // wrap in longitude
        if (hashCell(cell, epoch) > STORM_DENSITY * weight / .45) continue;
        float2 centre = (cell + float2(hashCell(cell, epoch + .3), hashCell(cell, epoch + .6))) * cellSize;
        float2 d = uv - centre;
        d.x -= round(d.x);
        d.x *= cosLat; // texture units to equator-equivalent, so a flash is round
        float r2 = dot(d, d);
        // The current slot and the previous, whose flash may still be decaying.
        float slot = floor(t / FLASH_SLOT);
        for (int k = 0; k < 2; ++k)
        {
            float s = slot - float(k);
            if (hashCell(cell, s * .01 + 3.0) > chance) continue;
            float start = (s + hashCell(cell, s * .01 + 7.0)) * FLASH_SLOT;
            float age = t - start;
            if (age < 0.0) continue;
            float envelope = exp(-age / FLASH_DECAY) * (1.0 - exp(-age / .012));
            // Some flashes strike twice.
            envelope += hashCell(cell, s * .01 + 9.0) < .4 ? .6 * exp(-max(age - .07, 0.0) / FLASH_DECAY) * step(.07, age) : 0.0;
            float strength = .5 + hashCell(cell, s * .01 + 11.0);
            float sigma = max(FLASH_CORE, max(footprint.x, footprint.y) * .7);
            float core = exp(-r2 / (2.0 * sigma * sigma)) * (FLASH_CORE * FLASH_CORE) / (sigma * sigma);
            float glow = exp(-r2 / (2.0 * FLASH_GLOW * FLASH_GLOW)) * .012;
            total += (core + glow) * envelope * strength;
        }
    }
    return total * life * night.y * 40.0 * float3(.85, .9, 1.0);
}

// A gas giant is lit by scattering, not a surface. The cloud deck itself is
// close to Lambert (Jupiter's continuum Minnaert exponent is 0.9 to 1.0),
// wrapped a few degrees past the terminator; over it lies a thin haze whose
// view path grows as 1/cos toward the limb. The haze is forward scattering,
// so at full phase it veils the deck with little light of its own and the
// limb darkens, at crescent phases it lights up into a rim, and being high
// it stays sunlit a little past the terminator, carrying the glow across.
float3 shadeGiant(Material m, float3 N, float3 geometricN, float3 L, float3 V, float2 uv, float shear)
{
    float4 haze = root.frame->giant_more;
    float wrap = haze.y;
    float lit = saturate((dot(N, L) + wrap) / (1.0 + wrap));
    float hazeWrap = wrap * 2.0 + .05;
    float hazeLit = saturate((dot(geometricN, L) + hazeWrap) / (1.0 + hazeWrap));
    float mu = max(dot(geometricN, V), .02);
    float veil = 1.0 - exp(-haze.x / mu);
    float phase = 4.0 * PI * henyeyGreenstein(-dot(L, V), .5); // relative to isotropic
    float3 hazeColour = float3(.55, .62, .74);
    float3 deck = m.albedo / PI * lit * (1.0 - veil);
    float3 scattered = hazeColour / PI * veil * hazeLit * phase;
    float3 colour = (deck + scattered) * sunlight() * m.visibility + m.albedo * float3(.003, .004, .008);
    // Lightning shows where the sun does not.
    float dark = 1.0 - smoothstep(-.12, .02, dot(geometricN, L));
    if (dark > 0.0)
    {
        float2 footprint = max(abs(ddx(uv)), abs(ddy(uv)));
        colour += giantLightning(uv, root.frame->camera_time.w, shear, footprint) * dark * (m.albedo * 1.5 + .25);
    }
    return colour;
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Surface s = surfaceAt(input, false);
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    Material m = giantMaterial(input, rotate, s.N, s.L, s.V, s.visibility);
    float2 uv = sphereUV(normalize(input.localNormal));
    float shear = sampleSphere(textures[GAS_FLOW], uv).b;
    return float4(shadeGiant(m, m.N, s.N, s.L, s.V, uv, shear), 1);
}
