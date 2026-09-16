#include "../scene/frame_bindings.slang"
#include "beltblur_shared.h"

// Separable blur of the belt transmittance map: horizontal reads the splat
// target and writes the scratch map, vertical reads the scratch map and
// writes back. The width approximates the sun's penumbra.

static const int BLUR_RADIUS = 4;
static const float BLUR_SIGMA = 2.0;

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    bool horizontal = root.mode == ORBITAL_BELT_BLUR_HORIZONTAL;
    float2 step = horizontal ? float2(1.0, 0.0) : float2(0.0, 1.0);
    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    float weights = 0.0;
    for (int i = -BLUR_RADIUS; i <= BLUR_RADIUS; ++i)
    {
        float w = exp(-float(i * i) / (2.0 * BLUR_SIGMA * BLUR_SIGMA));
        int2 texel = int2(position.xy + step * float(i));
        sum += (horizontal ? textures[TEX_BELT_LIGHT_MAP].Load(int3(texel, 0)) : textures[TEX_BELT_LIGHT_BLUR].Load(int3(texel, 0))) * w;
        weights += w;
    }
    return sum / weights;
}
