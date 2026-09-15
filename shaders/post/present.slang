#include "../scene/frame_bindings.slang"
#include "../lib/color.slang"

// The final image onto the swapchain. SDR: the composite's linear values, which
// the sRGB swapchain encodes. HDR: the intermediate holds the linear display
// value over the headroom; scale it by the headroom and the paper white and
// encode for the swapchain's colour space: scRGB is linear with 1.0 at 80
// nits, HDR10 is Rec. 2020 under the PQ curve normalized to 10,000 nits.
[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    float4 sample = textures[TEX_FINAL_IMAGE].Sample(samplers[SAMPLER_CLAMP], position.xy / resolution);
    float mode = root.frame.display.x;
    if (mode <= 0.0) return sample;
    float paperWhite = root.frame.display.y, headroom = max(root.frame.display.z, 1.0);
    float3 display = sample.rgb * headroom; // in units of the tone curve's white
    if (mode < 1.5) return float4(display * paperWhite / 80.0, 1.0);
    return float4(pqEncode(mul(REC709_TO_REC2020, display) * paperWhite / 10000.0), 1.0);
}
