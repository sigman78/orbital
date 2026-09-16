#include "chart.slang"
#include "../surface/lighting.slang"
#include "../surface/brdf.slang"

// The surface BRDF on a sphere per tile: columns sweep the phase angle from full
// (the light behind the viewer) to new (behind the sphere), rows the roughness
// on land, and the last row the ocean with its glint. Any change to the lighting
// terms shows on this sheet; the tiers' no-pop rule (lighting identical across
// tiers) is a diff of two of these.

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    ChartTile tile = chartTile(position.xy, ORBITAL_CHART_BRDF_COLUMNS, ORBITAL_CHART_BRDF_ROWS);
    float3 N;
    if (tile.column < 0 || !chartSphere(tile, N)) return float4(0.0, 0.0, 0.0, 1.0);
    float phase = PI * float(tile.column) / float(ORBITAL_CHART_BRDF_COLUMNS - 1);
    float3 L = normalize(float3(sin(phase), .3, cos(phase)));
    float3 V = float3(0.0, 0.0, 1.0);
    bool ocean = tile.row == ORBITAL_CHART_BRDF_ROWS - 1;
    static const float roughness[ORBITAL_CHART_BRDF_ROWS - 1] = {.05, .2, .4, .6, .85};
    Material m;
    m.albedo = ocean ? float3(.02, .05, .12) : float3(.4, .4, .4);
    m.N = N;
    m.rough = ocean ? max(root.frame.earth.x, .03) : roughness[min(tile.row, ORBITAL_CHART_BRDF_ROWS - 2)];
    m.water = ocean ? 1.0 : 0.0;
    m.visibility = 1.0;
    return float4(shadeSurface(m, L, V, sunlight(root.frame), root.frame.earth.y, root.frame.quality.w), 1.0);
}
