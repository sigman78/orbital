#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../lib/color.slang"

// The final image onto the swapchain. SDR: the composite's linear values, which
// the sRGB swapchain encodes. HDR: the intermediate holds the linear display
// value over the headroom; scale it by the headroom and the paper white and
// encode for the swapchain's colour space: scRGB is linear with 1.0 at 80
// nits, HDR10 is Rec. 2020 under the PQ curve normalized to 10,000 nits.
[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    ViewParams view = viewParams(root.frame);
    DisplayParams display = displayParams(root.frame);
    float2 resolution = view.size;
    float4 sample = textures[TEX_FINAL_IMAGE].Sample(samplers[SAMPLER_CLAMP], position.xy / resolution);
    int mode = display.hdr;
    if (mode <= 0) return sample;
    float paperWhite = display.paperWhite, headroom = max(display.headroom, 1.0);
    float3 displayLinear = sample.rgb * headroom; // in units of the tone curve's white
    if (mode < 2) return float4(displayLinear * paperWhite / 80.0, 1.0);
    return float4(pqEncode(mul(REC709_TO_REC2020, displayLinear) * paperWhite / 10000.0), 1.0);
}
