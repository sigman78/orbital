#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "sun_occlusion.slang"

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    ViewParams view = viewParams(root.frame);
    ScreenSun sunScreen = screenSun(root.frame);
    float2 resolution = view.size;
    float visibility = sunScreen.visible
        ? sunDepthVisibility(root.frame, textures[TEX_DEPTH], textures[TEX_SPLAT_MASK], sunScreen.position, float2(resolution.x / resolution.y, 1.0)) : 0.0;
    return float4(visibility, 0.0, 0.0, 1.0);
}
