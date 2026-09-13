#include "../scene/frame_bindings.slang"

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    return textures[TEX_FINAL_IMAGE].Sample(samplers[SAMPLER_CLAMP], position.xy / resolution);
}
