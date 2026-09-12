#include "common.slang"

// Root.mode values (PostMode on the C++ side).
static const uint MODE_TONEMAP = 0, MODE_BLOOM_A = 1, MODE_BLOOM_B = 2, MODE_PRESENT = 3, MODE_METER = 4, MODE_FXAA = 5;

// Small finishing effects on top of exposure, bloom and the ACES curve.
static const float VIGNETTE = .17;             // darkening at the frame corners
static const float ABERRATION = .00025;        // lateral colour fringe at the edges, in uv per cubic radius of a REFERENCE_FRAME
static const float2 REFERENCE_FRAME = float2(1600.0, 900.0); // the default window; pixel-sized effects are tuned here and keep their pixel size elsewhere
static const float GRAIN = .010;               // film grain amplitude in display space
static const float METER_CENTRE_WEIGHT = .6;   // how much the metering favours the frame centre

float3 sampleHDR(float2 uv) { return textures[16].Sample(samplers[0], uv).rgb; }

// FXAA over the tone-mapped image in textures[30] (the quality variant: edge
// end search plus sub-pixel blend), written to the final image in mode FXAA.
static const float FXAA_EDGE_MIN = .0312, FXAA_EDGE_RELATIVE = .125;
static const int FXAA_SEARCH_STEPS = 8;
static const float FXAA_SUBPIXEL = .5;
static const float FXAA_MIN_EDGE_TEXELS = 2.5; // shorter features are specks, not edges
static const float FXAA_SPECK_MAX_LUMA = .35;   // specks brighter than this are close-rock highlights

float fxaaLuma(float2 uv) { float3 c = textures[30].SampleLevel(samplers[0], uv, 0.0).rgb; return dot(c, float3(.299, .587, .114)); }

float4 fxaa(float2 uv, float2 texel)
{
    float4 centre = textures[30].SampleLevel(samplers[0], uv, 0.0);
    float lumaM = dot(centre.rgb, float3(.299, .587, .114));
    float lumaN = fxaaLuma(uv + float2(0, -texel.y)), lumaS = fxaaLuma(uv + float2(0, texel.y));
    float lumaW = fxaaLuma(uv + float2(-texel.x, 0)), lumaE = fxaaLuma(uv + float2(texel.x, 0));
    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float range = lumaMax - lumaMin;
    if (range < max(FXAA_EDGE_MIN, lumaMax * FXAA_EDGE_RELATIVE)) return centre;
    // A dim isolated speck, brighter than all four neighbours, is a distant
    // rock rather than an edge: it stays sharp, the temporal pass settles it.
    // Bright ones are highlights on close rocks, which still want the filter.
    float lumaNeighbourMax = max(max(lumaN, lumaS), max(lumaW, lumaE));
    if (lumaM > lumaNeighbourMax && lumaM < FXAA_SPECK_MAX_LUMA) return centre;
    float lumaNW = fxaaLuma(uv + float2(-texel.x, -texel.y)), lumaNE = fxaaLuma(uv + float2(texel.x, -texel.y));
    float lumaSW = fxaaLuma(uv + float2(-texel.x, texel.y)), lumaSE = fxaaLuma(uv + float2(texel.x, texel.y));
    float lumaNS = lumaN + lumaS, lumaWE = lumaW + lumaE;
    float lumaWCorners = lumaNW + lumaSW, lumaECorners = lumaNE + lumaSE;
    float lumaNCorners = lumaNW + lumaNE, lumaSCorners = lumaSW + lumaSE;
    float edgeHorizontal = abs(-2.0 * lumaW + lumaWCorners) + abs(-2.0 * lumaM + lumaNS) * 2.0 + abs(-2.0 * lumaE + lumaECorners);
    float edgeVertical = abs(-2.0 * lumaN + lumaNCorners) + abs(-2.0 * lumaM + lumaWE) * 2.0 + abs(-2.0 * lumaS + lumaSCorners);
    bool horizontal = edgeHorizontal >= edgeVertical;
    float luma1 = horizontal ? lumaN : lumaW, luma2 = horizontal ? lumaS : lumaE;
    float gradient1 = luma1 - lumaM, gradient2 = luma2 - lumaM;
    bool steepest1 = abs(gradient1) >= abs(gradient2);
    float gradientScaled = .25 * max(abs(gradient1), abs(gradient2));
    float stepLength = horizontal ? texel.y : texel.x;
    float lumaLocalAverage = steepest1 ? .5 * (luma1 + lumaM) : .5 * (luma2 + lumaM);
    if (steepest1) stepLength = -stepLength;
    float2 currentUV = uv;
    if (horizontal) currentUV.y += stepLength * .5; else currentUV.x += stepLength * .5;
    float2 offset = horizontal ? float2(texel.x, 0) : float2(0, texel.y);
    float2 uv1 = currentUV - offset, uv2 = currentUV + offset;
    float lumaEnd1 = fxaaLuma(uv1) - lumaLocalAverage, lumaEnd2 = fxaaLuma(uv2) - lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled, reached2 = abs(lumaEnd2) >= gradientScaled;
    for (int i = 0; i < FXAA_SEARCH_STEPS && !(reached1 && reached2); ++i)
    {
        float stride = 1.0 + float(i) * .5;
        if (!reached1) { uv1 -= offset * stride; lumaEnd1 = fxaaLuma(uv1) - lumaLocalAverage; reached1 = abs(lumaEnd1) >= gradientScaled; }
        if (!reached2) { uv2 += offset * stride; lumaEnd2 = fxaaLuma(uv2) - lumaLocalAverage; reached2 = abs(lumaEnd2) >= gradientScaled; }
    }
    float distance1 = horizontal ? uv.x - uv1.x : uv.y - uv1.y;
    float distance2 = horizontal ? uv2.x - uv.x : uv2.y - uv.y;
    bool direction1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;
    // The search ended within a couple of texels each way: a few-pixel rock, not an edge.
    if (edgeThickness < FXAA_MIN_EDGE_TEXELS * (horizontal ? texel.x : texel.y)) return centre;
    float pixelOffset = -distanceFinal / edgeThickness + .5;
    bool isLumaCentreSmaller = lumaM < lumaLocalAverage;
    bool correctVariation = ((direction1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCentreSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    // Sub-pixel aliasing: blend toward the 3x3 average where the centre stands out.
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaNS + lumaWE) + lumaWCorners + lumaECorners);
    float subPixelOffset1 = saturate(abs(lumaAverage - lumaM) / range);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    finalOffset = max(finalOffset, subPixelOffset2 * subPixelOffset2 * FXAA_SUBPIXEL);
    float2 finalUV = uv;
    if (horizontal) finalUV.y += finalOffset * stepLength; else finalUV.x += finalOffset * stepLength;
    return textures[30].SampleLevel(samplers[0], finalUV, 0.0);
}

// Lateral chromatic aberration: red and blue are sampled slightly outward and
// inward along the radius, growing quadratically toward the edges.
// The fringe is sized in pixels of the reference frame, so a larger or wider
// window does not grow it: the radius uses the reference aspect and the shift
// converts through the reference resolution.
float3 sampleAberrated(float2 uv, float2 p, float2 resolution)
{
    float2 radial = p * float2(REFERENCE_FRAME.x / REFERENCE_FRAME.y, 1.0);
    float2 shift = radial * dot(radial, radial) * ABERRATION * REFERENCE_FRAME / resolution;
    return float3(sampleHDR(uv + shift).r, sampleHDR(uv).g, sampleHDR(uv - shift).b);
}
// --- Tone mapping curves, selected by Frame.quality.y ---------------------------
// All take linear scene radiance in sRGB primaries and return display-linear
// colour in [0, 1]. Matrix products are written out as row dot products so
// the memory layout flag for buffer matrices cannot change their meaning.

static const uint TONE_ACES = 0, TONE_AGX = 1, TONE_NEUTRAL = 2;

// ACES filmic: Stephen Hill's fit of the RRT and sRGB ODT (sRGB to AP1, the
// fitted curve, AP1 back to sRGB).
float3 acesFilmic(float3 c)
{
    float3 v = float3(dot(float3(0.59719, 0.35458, 0.04823), c), dot(float3(0.07600, 0.90834, 0.01566), c),
                      dot(float3(0.02840, 0.13383, 0.83777), c));
    float3 a = v * (v + 0.0245786) - 0.000090537;
    float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    v = a / b;
    return saturate(float3(dot(float3(1.60475, -0.53108, -0.07367), v), dot(float3(-0.10208, 1.10813, -0.00605), v),
                           dot(float3(-0.00327, -0.07276, 1.07602), v)));
}

// AgX (Troy Sobotka), the minimal form: inset to a wider gamut, log2
// encode over the working range, a sixth-order sigmoid, outset, decode.
float3 agx(float3 c)
{
    float3 v = float3(dot(float3(0.842479062253094, 0.0423282422610123, 0.0423756549057051), c),
                      dot(float3(0.0784335999999992, 0.878468636469772, 0.0784336), c),
                      dot(float3(0.0792237451477643, 0.0791661274605434, 0.879142973793104), c));
    const float minEv = -12.47393, maxEv = 4.026069;
    v = clamp(log2(max(v, 1e-10)), minEv, maxEv);
    v = (v - minEv) / (maxEv - minEv);
    float3 v2 = v * v, v4 = v2 * v2;
    v = 15.5 * v4 * v2 - 40.14 * v4 * v + 31.96 * v4 - 6.868 * v2 * v + 0.4298 * v2 + 0.1191 * v - 0.00232;
    v = float3(dot(float3(1.19687900512017, -0.0528968517574562, -0.0529716355144438), v),
               dot(float3(-0.0980208811401368, 1.15190312990417, -0.0980434501171241), v),
               dot(float3(-0.0990297440797205, -0.0989611768448433, 1.15107367264116), v));
    return pow(saturate(v), 2.2); // the sigmoid lands in encoded space; back to display linear
}

// Khronos PBR Neutral: hue-preserving, linear until 0.8, then a soft
// shoulder with a slight desaturation toward white.
float3 pbrNeutral(float3 c)
{
    const float startCompression = 0.8 - 0.04, desaturation = 0.15;
    float x = min(c.r, min(c.g, c.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    c -= offset;
    float peak = max(c.r, max(c.g, c.b));
    if (peak < startCompression) return c;
    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    c *= newPeak / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return lerp(c, newPeak.xxx, g);
}

float3 toneMap(float3 c)
{
    uint curve = uint(root.frame.quality.y + .5);
    if (curve == TONE_AGX) return agx(c);
    if (curve == TONE_NEUTRAL) return pbrNeutral(c);
    return acesFilmic(c);
}

// Lens ghosts: a few soft, faintly tinted ellipses spaced along the axis from
// the sun through the frame centre, stretched across that axis, plus a very
// faint anamorphic streak. Everything scales with the sun's visibility.
// Fraction of the solar disc not covered by scene geometry: depth taps over
// the disc's screen footprint, so rocks and moons dim the glare and the
// flare as they cross the sun (the planets are handled analytically).
float sunDepthVisibility(float2 sun, float2 aspect)
{
    float2 uv = sun * .5 + .5;
    if (any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    float2 radius = root.frame.sun_disc.x / root.frame.right_tan.w / aspect * .5; // disc radius in uv
    float open = 0.0;
    for (int i = 0; i < 12; ++i)
    {
        float r = sqrt((float(i) + .5) / 12.0), a = float(i) * 2.399963;
        float2 tap = clamp(uv + float2(cos(a), sin(a)) * r * radius, 0.0, 1.0);
        open += textures[18].SampleLevel(samplers[0], tap, 0.0).r >= .999999 ? 1.0 : 0.0;
    }
    return open / 12.0;
}

// Ghost images down the lens axis: the sun's image reflected between lens
// elements lands as a few dim, soft-edged copies of the aperture (the blade
// polygon, or a circle with no blades), lightly tinted by the coatings and a
// touch brighter at the rim than in the body, as spherical aberration leaves
// them. Low contrast: they read as a veil over the picture, not as shapes.
float3 lensGhosts(float2 p, float2 sun, float vis, float2 aspect, int blades)
{
    float3 color = float3(0.0, 0.0, 0.0);
    const float offsets[4] = { .22, .55, .95, 1.40 };
    const float sizes[4] = { .06, .11, .08, .15 };
    const float3 tints[4] = { float3(.75, .85, 1.0), float3(1.0, .90, .75), float3(.80, 1.0, .90), float3(1.0, .80, .70) };
    for (int i = 0; i < 4; ++i)
    {
        float2 d = (p + sun * offsets[i]) * aspect / sizes[i];
        // Signed distance to the aperture: the largest projection onto the blade normals.
        float r = length(d);
        if (blades >= 3)
        {
            r = 0.0;
            for (int k = 0; k < 12; ++k)
            {
                if (k >= blades) break;
                float angle = float(k) * 2.0 * PI / float(blades) + .35;
                r = max(r, dot(d, float2(cos(angle), sin(angle))));
            }
        }
        float body = 1.0 - smoothstep(.7, 1.0, r);             // soft edge over the outer third
        float rim = exp(-pow((r - .85) / .12, 2.0)) * .5;       // the brighter rim
        color += tints[i] * vis * .008 * (body + rim);
    }
    return color;
}

// Aperture starburst: blades through the sun with chromatic dispersion along
// each streak, blue near the core and warm toward the tips.
float3 starburst(float2 p, float2 sun, float vis, float2 aspect, int blades, float strength)
{
    float3 color = float3(0.0, 0.0, 0.0);
    float2 d = (p - sun) * aspect;
    for (int k = 0; k < 12; ++k)
    {
        if (k >= blades) break;
        float angle = float(k) * PI / float(blades) + .35;
        float2 dir = float2(cos(angle), sin(angle));
        float along = abs(dot(d, dir)), perp = dot(d, float2(-dir.y, dir.x));
        float width = .0025 + .018 * along;
        float streak = exp(-(perp * perp) / (width * width)) * exp(-along * 2.6);
        float3 tint = lerp(float3(.75, .85, 1.0), float3(1.0, .62, .38), saturate(along * 1.6));
        color += tint * streak;
    }
    return color * vis * .012 * strength;
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    if (root.mode == MODE_METER) {
        // Four taps per meter cell, weighted toward the frame centre.
        float2 cell = 1.0 / 16.0;
        float2 uv = position.xy * cell;
        float3 c = sampleHDR(uv - cell * .25) + sampleHDR(uv + cell * float2(.25, -.25))
                 + sampleHDR(uv + cell * float2(-.25, .25)) + sampleHDR(uv + cell * .25);
        float lum = dot(c * .25, float3(.2126, .7152, .0722));
        float weight = 1.0 - METER_CENTRE_WEIGHT * smoothstep(0.0, 1.0, length(uv * 2.0 - 1.0));
        return float4(log(max(lum, .0001)), lum, weight, 1.0);
    }
    if (root.mode == MODE_FXAA)
        return fxaa(position.xy / resolution, 1.0 / resolution);
    if (root.mode == MODE_PRESENT)
        return textures[6].Sample(samplers[0], position.xy / resolution);
    if (root.mode == MODE_BLOOM_A || root.mode == MODE_BLOOM_B) {
        float2 uv = position.xy / max(floor(resolution * .25), float2(1.0, 1.0));
        float3 sum = float3(0.0, 0.0, 0.0);
        for (int i = -4; i <= 4; ++i) {
            // Tap spacing scales with the frame height, so the halo keeps its size relative to the picture.
            float2 delta = (root.mode == MODE_BLOOM_A ? float2(float(i), 0.0) : float2(0.0, float(i))) * 4.0 * (resolution.y / REFERENCE_FRAME.y) / resolution;
            float3 c = root.mode == MODE_BLOOM_A ? sampleHDR(uv + delta) : textures[1].Sample(samplers[0], uv + delta).rgb;
            if (root.mode == MODE_BLOOM_A) c = max(c - float3(.85, .85, .85), float3(0.0, 0.0, 0.0));
            sum += c * exp(-float(i * i) / 8.0);
        }
        return float4(sum / 4.898, 1.0);
    }
    float2 uv = position.xy / resolution;
    float2 p = uv * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float3 color = sampleAberrated(uv, p, resolution) + textures[2].Sample(samplers[0], uv).rgb * .24;
    float2 sun = root.frame.screen_sun.xy;
    float4 lens = root.frame.lens;
    float vis = root.frame.screen_sun.z * sunDepthVisibility(sun, aspect);
    float dist = length((p - sun) * aspect);
    color += float3(1.0, .6, .3) * vis * .014 * lens.x / (dist * dist + .012);
    color += lensGhosts(p, sun, vis, aspect, int(lens.w + .5)) * lens.y;
    color += starburst(p, sun, vis, aspect, int(lens.w + .5), lens.z);
    color = toneMap(color * root.frame.forward_exposure.w);
    color *= 1.0 - VIGNETTE * pow(length(p * .65), 2.0);
    // Dither and grain in display-code space; linear-space noise is far too
    // bright in black space. Grain is strongest in the midtones.
    float luminance = dot(color, float3(.2126, .7152, .0722));
    float grain = (hash(float3(position.xy, frac(root.frame.camera_time.w * 7.0) * 100.0)) - .5) * GRAIN
                  * (4.0 * luminance * (1.0 - luminance));
    float dither = (hash(float3(position.xy, 0.0)) - .5) / 255.0 + grain;
    if (root.frame.options.w > .5) {
        float2 hp = (position.xy - float2(32.0, 26.0)) / float2(1024.0, 256.0);
        if (all(hp >= float2(0.0, 0.0)) && all(hp < float2(1.0, 1.0))) {
            float a = textures[14].Sample(samplers[0], hp).a;
            color = lerp(color, float3(.68, .75, .81), a * .8);
        }
    }
    float3 encoded=lerp(12.92*color,1.055*pow(max(color,0.0),1.0/2.4)-.055,step(.0031308,color));
    encoded=clamp(encoded+dither,0.0,1.0);
    float3 decoded=lerp(encoded/12.92,pow((encoded+.055)/1.055,2.4),step(.04045,encoded));
    return float4(decoded,1.0);
}
