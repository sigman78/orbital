#include "hdr.slang"
#include "../lib/color.slang"

static const float METER_CENTRE_WEIGHT = .6; // fraction of centre-biased metering

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    // Four taps per meter cell, weighted toward the frame centre.
    float2 cell = 1.0 / 16.0;
    float2 uv = position.xy * cell;
    float3 c = sampleHDR(uv - cell * .25) + sampleHDR(uv + cell * float2(.25, -.25))
             + sampleHDR(uv + cell * float2(-.25, .25)) + sampleHDR(uv + cell * .25);
    float lum = luma709(c * .25);
    float weight = 1.0 - METER_CENTRE_WEIGHT * smoothstep(0.0, 1.0, length(uv * 2.0 - 1.0));
    return float4(log(max(lum, .0001)), lum, weight, 1.0);
}
