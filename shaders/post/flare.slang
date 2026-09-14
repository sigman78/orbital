#include "../scene/frame_bindings.slang"
#include "lens.slang"

// The soft part of the lens flare stack into a target at the frame over the
// panel's divisor (2, 4 or 8); at a quarter nothing in it is sharper than a
// few of its texels, so the composite's cubic sample resolves it, and the
// eighth reads as a defocused stack. The thin streak, the starburst and the glare
// stay at full resolution in the composite.
[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    float2 size = max(floor(resolution / root.frame.lens_stack.x), float2(1.0, 1.0));
    float2 p = position.xy / size * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float vis = root.frame.screen_sun.z * textures[TEX_SUN_VISIBILITY].Load(int3(0, 0, 0)).r;
    float4 strengths = float4(root.frame.sun_disc.z, root.frame.sun_disc.w, root.frame.lens_more.x, root.frame.lens.y);
    float3 tune = float3(root.frame.lens_more.z, root.frame.lens_more.w, root.frame.lens_more.y);
    return float4(lensStack(p, root.frame.screen_sun.xy, vis, aspect, strengths, tune), 1.0);
}
