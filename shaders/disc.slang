#include "common.slang"
#include "beltfar.slang"

// Far-belt sunlight bake: the light reaching each texel of the belt plane
// through the belt and the planets' shadows, refreshed every few frames while
// the far tier is in use. The far tier itself is drawn by dust.slang, which
// composites it with the near march in one blend.

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    return bakeSunlight(position.xy);
}
