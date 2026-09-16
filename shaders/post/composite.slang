#include "../lib/tonemapping.slang"
#include "lens.slang"
#include "hdr.slang"
#include "../lib/noise.slang"
#include "../lib/color.slang"
#include "../scene/frame_records.slang"

// Finishing effects applied around exposure, bloom and the selected tone curve.
static const float ABERRATION = .00025;        // lateral colour fringe at the edges, in uv per cubic radius of a REFERENCE_FRAME, at the panel's 1x
static const float2 REFERENCE_FRAME = float2(1600.0, 900.0); // the default window; pixel-sized effects are tuned here and keep their pixel size elsewhere

// Lateral chromatic aberration: red and blue are sampled slightly outward and
// inward along the radius, growing quadratically toward the edges.
// The fringe is sized in pixels of the reference frame, so a larger or wider
// window does not grow it: the radius uses the reference aspect and the shift
// converts through the reference resolution.
float3 sampleAberrated(float2 uv, float2 p, float2 resolution)
{
    float2 radial = p * float2(REFERENCE_FRAME.x / REFERENCE_FRAME.y, 1.0);
    float2 shift = radial * dot(radial, radial) * ABERRATION * postParams(root.frame).aberration * REFERENCE_FRAME / resolution;
    return float3(sampleHDR(uv + shift).r, sampleHDR(uv).g, sampleHDR(uv - shift).b);
}

// Cubic B-spline upsample as four bilinear taps: plain bilinear leaves a
// diamond pattern when the coarse flare target is enlarged eight or sixteen
// times, and this stays smooth across texel centres.
float3 sampleCubic(Texture2D<float4> image, float2 uv, float2 size)
{
    float2 texel = uv * size - .5;
    float2 f = frac(texel), centre = floor(texel) + .5;
    float2 f2 = f * f, f3 = f2 * f;
    float2 w0 = (-f3 + 3.0 * f2 - 3.0 * f + 1.0) / 6.0, w1 = (3.0 * f3 - 6.0 * f2 + 4.0) / 6.0;
    float2 w2 = (-3.0 * f3 + 3.0 * f2 + 3.0 * f + 1.0) / 6.0, w3 = f3 / 6.0;
    float2 g0 = w0 + w1, g1 = w2 + w3;
    float2 h0 = (centre - 1.0 + w1 / g0) / size, h1 = (centre + 1.0 + w3 / g1) / size;
    return (image.Sample(samplers[SAMPLER_CLAMP], float2(h0.x, h0.y)).rgb * g0.x
            + image.Sample(samplers[SAMPLER_CLAMP], float2(h1.x, h0.y)).rgb * g1.x) * g0.y
           + (image.Sample(samplers[SAMPLER_CLAMP], float2(h0.x, h1.y)).rgb * g0.x
              + image.Sample(samplers[SAMPLER_CLAMP], float2(h1.x, h1.y)).rgb * g1.x) * g1.y;
}

// Dirty glass: a film of dust, wipe residue and smears on a convex pane in
// front of the camera. Looking toward the light the marks are nearly
// invisible and only blur what is seen through them; where the sun grazes
// the pane they brighten. The pane's outward curvature tilts its normal
// toward the frame edges, so the grazing band is a region of the frame that
// moves as the camera turns, not the whole screen. The sun's true direction
// drives this, so it works with the sun far outside the frame.
static const float PANE_CURVATURE = .6; // the normal's tilt at the frame's edge, as a slope
float3 dirtyGlass(float3 color, float3 bloom, float mask, float2 uv, float2 p, float2 pixel, float2 resolution, float2 aspect)
{
    LensParams lens = lensParams(root.frame);
    PostParams post = postParams(root.frame);
    ViewParams view = viewParams(root.frame);
    float blur = saturate(mask * 2.5) * lens.dirtBlur;
    if (blur > 0.0) {
        // A small disc of scene taps, its radius growing with the film's density;
        // a per-pixel rotation of the fixed pattern hides its eight points.
        float radius = 10.0 * saturate(mask * 2.0) * min(lens.dirtBlur, 1.0) * resolution.y / REFERENCE_FRAME.y;
        float rotation = hash(float3(pixel, 3.0)) * 2.0 * PI;
        float3 sum = float3(0.0);
        for (int i = 0; i < 8; ++i) {
            float angle = rotation + float(i) * 2.399963;
            float2 offset = float2(cos(angle), sin(angle)) * sqrt((float(i) + .5) / 8.0) * radius / resolution;
            sum += sampleHDR(uv + offset);
        }
        color = lerp(color, sum / 8.0 + bloom * post.bloom, saturate(blur));
    }
    if (lens.dirtLight > 0.0) {
        float3 sun = normalize(lightParams(root.frame).position);
        float3 light = float3(dot(sun, view.right), dot(sun, view.up), dot(sun, view.forward));
        float3 normal = normalize(float3(p.x * aspect.x * PANE_CURVATURE, -p.y * PANE_CURVATURE, 1.0));
        float incidence = dot(normal, light);
        // Lit from outside only, brightest just past grazing, gone facing the sun.
        float grazing = smoothstep(0.0, .12, incidence) * pow(1.0 - saturate(incidence), 3.0);
        color += float3(1.0, .9, .78) * mask * (.06 * grazing + .4 * bloom) * lens.dirtLight;
    }
    return color;
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    ViewParams view = viewParams(root.frame);
    LensParams lens = lensParams(root.frame);
    PostParams post = postParams(root.frame);
    DisplayParams display = displayParams(root.frame);
    ScreenSun sunScreen = screenSun(root.frame);
    float2 resolution = view.size;
    float2 uv = position.xy / resolution;
    float2 p = uv * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float3 color = sampleAberrated(uv, p, resolution);
    // The bloom is also the light that brings up the dirty glass, when on.
    float3 bloom = post.bloom > 0.0 ? textures[TEX_BLOOM_A].Sample(samplers[SAMPLER_CLAMP], uv).rgb : float3(0.0);
    color += bloom * post.bloom;
    if (lens.dirtLight > 0.0 || lens.dirtBlur > 0.0) {
        float mask = textures[TEX_LENS_DIRT].Sample(samplers[SAMPLER_CLAMP], uv).r;
        if (mask > .01) color = dirtyGlass(color, bloom, mask, uv, p, position.xy, resolution, aspect);
    }
    float2 sun = sunScreen.position;
    float vis = float(sunScreen.visible) * textures[TEX_SUN_VISIBILITY].Load(int3(0, 0, 0)).r;
    float dist = length((p - sun) * aspect);
    // The lens veil: the sun's glare and the stack (starburst and streak here,
    // the soft elements from flare.slang's fractional target), the panel's
    // saturation scaling the stack's colour. Kept apart from the scene so the
    // neutral curve's black offset does not react to it and the flare stays
    // purely additive.
    float3 flare = starburst(p, sun, vis, aspect, int(lens.starburstBlades + .5), lens.starburst)
                   + lensStreak(p, sun, vis, aspect, lens.streak)
                   + (lens.stackDivisor > 0.0 // the stack pass is skipped when the flare is off
                          ? sampleCubic(textures[TEX_FLARE], uv, floor(resolution / lens.stackDivisor))
                          : float3(0.0));
    // The glare: a 1/d^2 core with its far tail cut, since with the flare
    // purely additive nothing subtracts the tail and it veiled whole frames.
    float3 veil = float3(1.0, .6, .3) * vis * .014 * lens.glare / (dist * dist + .012) * exp(-dist * 1.5)
                  + lerp(float3(luma709(flare)), flare, lens.saturation);
    float exposure = view.exposure;
    // The flare is a display-level veil tuned against the scene at 1x: it follows the
    // manual exposure but only as much of the auto exposure as the panel allows, so a
    // sun in frame stopping the scene down does not take the flare with it.
    float veilExposure = exposure * pow(max(display.autoExposure, 1e-3), post.flareAdaptation - 1.0);
    // HDR output: the curve's shoulder reaches the headroom (peak over paper
    // white) instead of the curve's white; the 16-bit float intermediate then
    // holds the linear display value over the headroom, so the spatial
    // anti-aliasing, grain and dither see the same 0..1 values they do in SDR,
    // and the present pass scales it by the paper white.
    bool hdr = display.hdr > 0;
    float headroom = hdr ? max(display.headroom, 1.0) : 1.0;
    uint curve = uint(post.toneCurve);
    color = toneMap(color * exposure, veil * veilExposure, curve, post.blackOffset, headroom);
    color *= 1.0 - post.vignette * pow(length(p * .65), 2.0);
    color /= headroom; // from here on the display value over the headroom, 0..1
    // Dither and grain in display-code space; linear-space noise is far too
    // bright in black space. Grain is strongest in the midtones.
    float luminance = luma709(color);
    float grain = (hash(float3(position.xy, frac(view.time * 7.0) * 100.0)) - .5) * post.grain
                  * (4.0 * luminance * (1.0 - luminance));
    float dither = (hash(float3(position.xy, 0.0)) - .5) / 255.0 + grain;
    if (view.hud) {
        float2 hp = (position.xy - float2(32.0, 26.0)) / float2(1024.0, 256.0);
        if (all(hp >= float2(0.0, 0.0)) && all(hp < float2(1.0, 1.0))) {
            float a = textures[TEX_HUD].Sample(samplers[SAMPLER_CLAMP], hp).a;
            color = lerp(color, float3(.68, .75, .81) / headroom, a * .8);
        }
    }
    float3 encoded=linearToSRGB(color);
    encoded=clamp(encoded+dither,0.0,1.0);
    float3 decoded=sRGBToLinear(encoded);
    return float4(decoded,1.0);
}
