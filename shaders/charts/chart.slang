#pragma once
#include "../scene/frame_bindings.slang"
#include "../lib/math.slang"

// Charts: a full-screen pass drawn in place of the scene, a grid of tiles over
// the frame below the legend band, each tile one point of a parameter range,
// through the production functions. A chart is a shot like any other, so it goes
// through the real tone map and into the checks as one image per subject.

struct ChartTile {
    int column, row;   // the tile, or -1 outside the grid
    float2 local;      // 0..1 across the tile
    float2 pixelSize;  // the tile's size in pixels
};

ChartTile chartTile(float2 pixel, int columns, int rows)
{
    ChartTile tile;
    float2 frame = root.frame.options.xy;
    float2 origin = float2(float(ORBITAL_CHART_MARGIN), float(ORBITAL_CHART_TOP));
    float2 area = frame - origin - float(ORBITAL_CHART_MARGIN);
    tile.pixelSize = area / float2(columns, rows);
    float2 grid = (pixel - origin) / tile.pixelSize;
    tile.column = int(floor(grid.x));
    tile.row = int(floor(grid.y));
    if (any(pixel < origin) || tile.column >= columns || tile.row >= rows) tile.column = tile.row = -1;
    tile.local = frac(grid);
    return tile;
}

// A sphere inscribed in the tile: the normal at the pixel, or false outside the disc.
bool chartSphere(ChartTile tile, out float3 normal)
{
    float2 p = (tile.local - .5) * tile.pixelSize;
    float radius = .46 * min(tile.pixelSize.x, tile.pixelSize.y);
    float2 q = p / radius;
    float rr = dot(q, q);
    normal = float3(q.x, -q.y, sqrt(max(1.0 - rr, 0.0))); // y up on screen, z toward the viewer
    return rr < 1.0;
}

// The direction on the unit sphere for a map coordinate, the inverse of sphereUV.
float3 chartDirection(float2 uv)
{
    float longitude = (uv.x - .5) * 2.0 * PI, latitude = (.5 - uv.y) * PI;
    return float3(cos(latitude) * cos(longitude), sin(latitude), cos(latitude) * sin(longitude));
}
