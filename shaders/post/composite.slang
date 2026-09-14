#include "../lib/tonemapping.slang"
#include "lens.slang"
#include "hdr.slang"
#include "../lib/noise.slang"
#include "../lib/color.slang"

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
    float2 shift = radial * dot(radial, radial) * ABERRATION * root.frame.post.w * REFERENCE_FRAME / resolution;
    return float3(sampleHDR(uv + shift).r, sampleHDR(uv).g, sampleHDR(uv - shift).b);
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    float2 uv = position.xy / resolution;
    float2 p = uv * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float3 color = sampleAberrated(uv, p, resolution);
    if (root.frame.post.x > 0.0) color += textures[TEX_BLOOM_A].Sample(samplers[SAMPLER_CLAMP], uv).rgb * root.frame.post.x;
    float2 sun = root.frame.screen_sun.xy;
    float4 lens = root.frame.lens;
    float vis = root.frame.screen_sun.z * textures[TEX_SUN_VISIBILITY].Load(int3(0, 0, 0)).r;
    float dist = length((p - sun) * aspect);
    color += float3(1.0, .6, .3) * vis * .014 * lens.x / (dist * dist + .012);
    color += lensGhosts(p, sun, vis, aspect, int(lens.w + .5)) * lens.y;
    color += starburst(p, sun, vis, aspect, int(lens.w + .5), lens.z);
    color += lensHalo(p, sun, vis, aspect, root.frame.sun_disc.zw);
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
