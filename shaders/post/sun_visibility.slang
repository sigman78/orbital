#include "../scene/frame_bindings.slang"
#include "sun_occlusion.slang"

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    float visibility = root.frame.screen_sun.z > 0.0
        ? sunDepthVisibility(root.frame, textures[TEX_DEPTH], textures[TEX_SPLAT_MASK], root.frame.screen_sun.xy, float2(resolution.x / resolution.y, 1.0)) : 0.0;
    return float4(visibility, 0.0, 0.0, 1.0);
}
