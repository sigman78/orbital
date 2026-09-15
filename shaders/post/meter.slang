#include "hdr.slang"
#include "../lib/color.slang"

// The exposure meter: a 16x16 grid of cells, each integrating its whole area
// of the resolved HDR image at a four pixel stride (bilinear taps, so nothing
// eight pixels or wider is missed: the sun's disc, a planet's limb), plus the
// sun's glare as the composite adds it, since the glare is what makes a sun in
// frame bright. Space votes like everything else; the CPU side averages the
// cells' means by their centre weight and adds a share of the brightest cell.
static const float METER_CENTRE_WEIGHT = .6; // fraction of centre-biased metering
static const float METER_STRIDE = 4.0;       // pixels between taps

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    float2 cell = 1.0 / 16.0;
    float2 origin = (position.xy - 1.0) * cell; // this cell's uv range: origin to origin + cell
    int2 taps = int2(ceil(resolution * cell / METER_STRIDE));
    float sum = 0.0, peak = 0.0;
    for (int y = 0; y < taps.y; ++y)
        for (int x = 0; x < taps.x; ++x) {
            float2 uv = origin + min((float2(x, y) + .5) * METER_STRIDE / resolution, cell);
            float lum = luma709(sampleHDR(uv));
            sum += lum;
            peak = max(peak, lum);
        }
    float mean = sum / float(taps.x * taps.y);
    // The sun's glare over this cell, as composite.slang draws it.
    float2 centre = origin + cell * .5;
    float2 p = centre * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float vis = root.frame.screen_sun.z * textures[TEX_SUN_VISIBILITY].Load(int3(0, 0, 0)).r;
    float dist = length((p - root.frame.screen_sun.xy) * aspect);
    float glare = luma709(float3(1.0, .6, .3)) * vis * .014 * root.frame.lens.x / (dist * dist + .012) * exp(-dist * 1.5);
    mean += glare;
    float weight = 1.0 - METER_CENTRE_WEIGHT * smoothstep(0.0, 1.0, length(p));
    return float4(mean, peak, weight, 1.0);
}
