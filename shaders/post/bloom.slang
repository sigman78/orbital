#include "bloom_shared.h"
#include "hdr.slang"
#include "../scene/frame_records.slang"

static const float2 REFERENCE_FRAME = float2(1600.0, 900.0); // halo size reference

// Bloom prefilter: the fraction of a pixel that blooms. Above the threshold
// plus the knee it is everything over the threshold, as the hard clip was;
// across the knee the curve bends smoothly to zero, so a source crossing the
// threshold fades its halo in over a band of brightness rather than popping
// it on. Weighted by the brightest channel so the halo keeps the source's hue
// instead of drifting toward its saturated channel.
float bloomWeight(float3 c)
{
    PostParams post = postParams(root.frame);
    float threshold = post.bloomThreshold, knee = max(post.bloomKnee, .0001);
    float brightness = max(c.r, max(c.g, c.b));
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    return max(soft, brightness - threshold) / max(brightness, .0001);
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = viewParams(root.frame).size;
    if (root.mode == ORBITAL_BLOOM_PREFILTER) {
        float2 size = max(floor(resolution * .25), float2(1.0, 1.0));
        float3 sum = float3(0.0, 0.0, 0.0);
        // Integrate the source area before decimating in either axis. Usually
        // 4x4 texels; fractional edge weights also conserve small-source energy
        // at odd frame sizes. Threshold each source texel before averaging.
        float2 first = (position.xy - .5) * resolution / size;
        float2 last = (position.xy + .5) * resolution / size;
        for (int y = int(floor(first.y)); y < int(ceil(last.y)); ++y)
        for (int x = int(floor(first.x)); x < int(ceil(last.x)); ++x) {
            float2 overlap = max(min(last, float2(x + 1, y + 1)) - max(first, float2(x, y)), 0.0);
            float3 c = max(sampleHDR((float2(x, y) + .5) / resolution), 0.0);
            sum += c * bloomWeight(c) * overlap.x * overlap.y;
        }
        float2 footprint = last - first;
        return float4(sum / (footprint.x * footprint.y), 1.0);
    }
    if (root.mode == ORBITAL_BLOOM_HORIZONTAL || root.mode == ORBITAL_BLOOM_VERTICAL) {
        float2 size = max(floor(resolution * .25), float2(1.0, 1.0));
        float2 uv = position.xy / size;
        bool horizontal = root.mode == ORBITAL_BLOOM_HORIZONTAL;
        // Keep the reference halo width, but grow the number of adjacent texel
        // taps instead of their spacing: sparse taps produce a grid at fullscreen.
        float sigma = max(.5, 8.0 * resolution.y / REFERENCE_FRAME.y * (horizontal ? size.x / resolution.x : size.y / resolution.y));
        int radius = int(ceil(2.0 * sigma));
        float3 sum = float3(0.0, 0.0, 0.0);
        float weightSum = 0.0;
        for (int i = -radius; i <= radius; ++i) {
            float2 delta = (horizontal ? float2(float(i), 0.0) : float2(0.0, float(i))) / size;
            float3 c = textures[horizontal ? TEX_BLOOM_A : TEX_BLOOM_B].SampleLevel(samplers[SAMPLER_CLAMP], uv + delta, 0.0).rgb;
            float weight = exp(-.5 * float(i * i) / (sigma * sigma));
            sum += c * weight;
            weightSum += weight;
        }
        return float4(sum / weightSum, 1.0);
    }
    return float4(0.0, 0.0, 0.0, 1.0);
}
