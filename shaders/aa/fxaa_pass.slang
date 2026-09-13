#include "../scene/frame_bindings.slang"
#include "fxaa.slang"

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    return fxaa(textures[TEX_LDR], samplers[SAMPLER_CLAMP], position.xy / resolution, 1.0 / resolution);
}
