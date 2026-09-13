#include "../lib/tonemapping.slang"
#include "../aa/fxaa.slang"
#include "lens.slang"
#include "../scene/frame_bindings.slang"
#include "../lib/noise.slang"
#include "../lib/color.slang"

// Root.mode values (PostMode on the C++ side).
static const uint MODE_TONEMAP = 0, MODE_BLOOM_A = 1, MODE_BLOOM_B = 2, MODE_PRESENT = 3, MODE_METER = 4, MODE_FXAA = 5;

// Finishing effects applied around exposure, bloom and the selected tone curve.
static const float ABERRATION = .00025;        // lateral colour fringe at the edges, in uv per cubic radius of a REFERENCE_FRAME, at the panel's 1x
static const float2 REFERENCE_FRAME = float2(1600.0, 900.0); // the default window; pixel-sized effects are tuned here and keep their pixel size elsewhere
static const float METER_CENTRE_WEIGHT = .6;   // how much the metering favours the frame centre

float3 sampleHDR(float2 uv) { return textures[TEX_HISTORY_A].Sample(samplers[SAMPLER_CLAMP], uv).rgb; }

// Lateral chromatic aberration: red and blue are sampled slightly outward and
// inward along the radius, growing quadratically toward the edges.
// The fringe is sized in pixels of the reference frame, so a larger or wider
// window does not grow it: the radius uses the reference aspect and the shift
// converts through the reference resolution.
float3 sampleAberrated(float2 uv, float2 p, float2 resolution)
{
    float2 radial = p * float2(REFERENCE_FRAME.x / REFERENCE_FRAME.y, 1.0);
    float2 shift = radial * dot(radial, radial) * ABERRATION * root.frame.post.w * REFERENCE_FRAME / resolution;
    return float3(sampleHDR(uv + shift).r, sampleHDR(uv).g, sampleHDR(uv - shift).b);
}
// Bloom prefilter: the fraction of a pixel that blooms. Above the threshold
// plus the knee it is everything over the threshold, as the hard clip was;
// across the knee the curve bends smoothly to zero, so a source crossing the
// threshold fades its halo in over a band of brightness rather than popping
// it on. Weighted by the brightest channel so the halo keeps the source's hue
// instead of drifting toward its saturated channel.
float bloomWeight(float3 c)
{
    float threshold = root.frame.post.y, knee = max(root.frame.post.z, .0001);
    float brightness = max(c.r, max(c.g, c.b));
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    return max(soft, brightness - threshold) / max(brightness, .0001);
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
        float lum = luma709(c * .25);
        float weight = 1.0 - METER_CENTRE_WEIGHT * smoothstep(0.0, 1.0, length(uv * 2.0 - 1.0));
        return float4(log(max(lum, .0001)), lum, weight, 1.0);
    }
    if (root.mode == MODE_FXAA)
        return fxaa(textures[TEX_LDR], samplers[SAMPLER_CLAMP], position.xy / resolution, 1.0 / resolution);
    if (root.mode == MODE_PRESENT)
        return textures[TEX_FINAL_IMAGE].Sample(samplers[SAMPLER_CLAMP], position.xy / resolution);
    if (root.mode == MODE_BLOOM_A || root.mode == MODE_BLOOM_B) {
        float2 uv = position.xy / max(floor(resolution * .25), float2(1.0, 1.0));
        float3 sum = float3(0.0, 0.0, 0.0);
        for (int i = -4; i <= 4; ++i) {
            // Tap spacing scales with the frame height, so the halo keeps its size relative to the picture.
            float2 delta = (root.mode == MODE_BLOOM_A ? float2(float(i), 0.0) : float2(0.0, float(i))) * 4.0 * (resolution.y / REFERENCE_FRAME.y) / resolution;
            float3 c = root.mode == MODE_BLOOM_A ? sampleHDR(uv + delta) : textures[TEX_BLOOM_A].Sample(samplers[SAMPLER_CLAMP], uv + delta).rgb;
            if (root.mode == MODE_BLOOM_A) c *= bloomWeight(c);
            sum += c * exp(-float(i * i) / 8.0);
        }
        return float4(sum / 4.898, 1.0);
    }
    float2 uv = position.xy / resolution;
    float2 p = uv * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float3 color = sampleAberrated(uv, p, resolution);
    if (root.frame.post.x > 0.0) color += textures[TEX_BLOOM_B].Sample(samplers[SAMPLER_CLAMP], uv).rgb * root.frame.post.x;
    float2 sun = root.frame.screen_sun.xy;
    float4 lens = root.frame.lens;
    float vis = root.frame.screen_sun.z * sunDepthVisibility(root.frame, textures[TEX_DEPTH], samplers[SAMPLER_CLAMP], sun, aspect);
    float dist = length((p - sun) * aspect);
    color += float3(1.0, .6, .3) * vis * .014 * lens.x / (dist * dist + .012);
    color += lensGhosts(p, sun, vis, aspect, int(lens.w + .5)) * lens.y;
    color += starburst(p, sun, vis, aspect, int(lens.w + .5), lens.z);
    color = toneMap(color * root.frame.forward_exposure.w, uint(root.frame.quality.y + .5), root.frame.post_more.z);
    color *= 1.0 - root.frame.post_more.x * pow(length(p * .65), 2.0);
    // Dither and grain in display-code space; linear-space noise is far too
    // bright in black space. Grain is strongest in the midtones.
    float luminance = luma709(color);
    float grain = (hash(float3(position.xy, frac(root.frame.camera_time.w * 7.0) * 100.0)) - .5) * root.frame.post_more.y
                  * (4.0 * luminance * (1.0 - luminance));
    float dither = (hash(float3(position.xy, 0.0)) - .5) / 255.0 + grain;
    if (root.frame.options.w > .5) {
        float2 hp = (position.xy - float2(32.0, 26.0)) / float2(1024.0, 256.0);
        if (all(hp >= float2(0.0, 0.0)) && all(hp < float2(1.0, 1.0))) {
            float a = textures[TEX_HUD].Sample(samplers[SAMPLER_CLAMP], hp).a;
            color = lerp(color, float3(.68, .75, .81), a * .8);
        }
    }
    float3 encoded=linearToSRGB(color);
    encoded=clamp(encoded+dither,0.0,1.0);
    float3 decoded=sRGBToLinear(encoded);
    return float4(decoded,1.0);
}
