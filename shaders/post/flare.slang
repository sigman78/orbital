#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "lens.slang"

// The soft part of the lens flare stack into a target at the frame over the
// panel's divisor (2, 4 or 8); at a quarter nothing in it is sharper than a
// few of its texels, so the composite's cubic sample resolves it, and the
// eighth reads as a defocused stack. The thin streak, the starburst and the glare
// stay at full resolution in the composite.
[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    ViewParams view = viewParams(root.frame);
    LensParams lens = lensParams(root.frame);
    ScreenSun sunScreen = screenSun(root.frame);
    float2 resolution = view.size;
    float2 size = max(floor(resolution / lens.stackDivisor), float2(1.0, 1.0));
    float2 p = position.xy / size * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float vis = float(sunScreen.visible) * textures[TEX_SUN_VISIBILITY].Load(int3(0, 0, 0)).r;
    float4 strengths = float4(lens.mainRing, lens.bigCrescent, lens.miniCrescents, lens.ghosts);
    float3 tune = float3(lens.ghostSpread, lens.ghostSize, lens.streak);
    return float4(lensStack(p, sunScreen.position, vis, aspect, strengths, tune), 1.0);
}
