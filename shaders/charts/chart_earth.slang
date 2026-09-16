#include "chart.slang"
#include "../planets/earth_material.slang"

// The Earth's ground shading on a flat window of its map per tile, through the
// production material: columns sweep the sun's elevation over the patch from
// overhead to below the horizon (the terminator and the city lights in the last
// two), rows the texels per pixel (the window widens, so the derivative-driven
// terms minify) at the full draw tier and then at the smallest, so a step
// between two rows is a tier pop and a step along a column a lighting fault.

static const float2 EARTH_CHART_CENTRE = float2(.56, .36); // the window's centre on the map
static const float elevation[ORBITAL_CHART_EARTH_COLUMNS] = {90.0, 60.0, 30.0, 10.0, 0.0, -10.0};
static const float texelsPerPixel[ORBITAL_CHART_EARTH_ROWS / 2] = {.5, 1.0, 2.0, 4.0};

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    ChartTile tile = chartTile(position.xy, ORBITAL_CHART_EARTH_COLUMNS, ORBITAL_CHART_EARTH_ROWS);
    if (tile.column < 0) return float4(0.0, 0.0, 0.0, 1.0);
    int ratioRow = tile.row % (ORBITAL_CHART_EARTH_ROWS / 2);
    float detail = tile.row < ORBITAL_CHART_EARTH_ROWS / 2 ? 1.0 : 0.0;
    // The map window: this many texels per pixel across the tile, square texels.
    float2 windowTexels = tile.pixelSize * texelsPerPixel[ratioRow];
    float2 uv = EARTH_CHART_CENTRE + (tile.local - .5) * windowTexels / EARTH_TEXELS;
    float3 localN = chartDirection(uv);
    float3x3 rotate = float3x3(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
    float3 N = localN;
    // East along the surface, for the sun to rise from.
    float3 east = normalize(cross(float3(0.0, 1.0, 0.0), N));
    float angle = elevation[tile.column] * PI / 180.0;
    float3 L = normalize(N * sin(angle) + east * cos(angle));
    // The view 20 degrees off the normal from the west: straight down would put
    // every water pixel at the mirror angle under an overhead sun, which no view has.
    float3 V = normalize(N * cos(20.0 * PI / 180.0) - east * sin(20.0 * PI / 180.0));
    return float4(earthGround(localN, rotate, N, L, V, 1.0, detail, ORBITAL_ROOT_FOLD_CLOUDS), 1.0);
}
