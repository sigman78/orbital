#pragma once
#include "scene/geometry.hpp"
#include "scene/terrain.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace space {

// Cube-sphere patches: six quadtrees sharing a grid, with skirts for seam fallback.
// Geometry uses body-local coordinates in radii.

struct PatchKey {
    std::uint8_t face = 0, level = 0;
    std::uint16_t x = 0, y = 0; // the cell within the face at this level, 0 .. 2^level - 1
    constexpr bool operator==(const PatchKey&) const = default;
    constexpr std::uint32_t packed() const { return face | level << 3 | x << 8 | y << 20; }
    constexpr PatchKey child(unsigned i) const {
        return {face, std::uint8_t(level + 1), std::uint16_t(2 * x + (i & 1)), std::uint16_t(2 * y + (i >> 1))};
    }
};

constexpr unsigned patch_quads = 16, patch_side = patch_quads + 1;
constexpr unsigned patch_vertex_count = patch_side * patch_side + 4 * patch_side; // the grid, then the skirt
constexpr unsigned patch_index_count = (patch_quads * patch_quads + 4 * patch_quads) * 6;
constexpr unsigned patch_level_max = 10; // a cell of 90 degrees over 1024, quads of 0.1 mrad
constexpr unsigned tile_side = 33;       // texels per tile edge, covering 32 quads
// Colour resolution per height texel. Ratio 2 matches the far maps at level 2
// and uses four times the colour memory of ratio 1.
constexpr unsigned tile_colour_ratio = 2;
constexpr unsigned tile_colour_side = (tile_side - 1) * tile_colour_ratio + 1;
// Slope range in radii per radian. Includes measured terrain and micro-relief
// gradients, with 16-bit precision resolving about 0.005 degrees.
constexpr float tile_slope_scale = 3.f;
// Radial skirt drop, also included in culling bounds.
constexpr float patch_skirt_drop = .002f;

// Direction of a face point, s and t in [-1, 1], warped by Everitt's mapping
// (tan(k*s)/tan(k), k=0.8687) so texel areas stay within about 11 percent.
Vec3d cube_direction(unsigned face, double s, double t);

struct CubeCoord {
    unsigned face;
    double s, t; // in [-1, 1]
};
CubeCoord cube_coordinates(Vec3d direction);

// Project the face's s axis onto the local sphere tangent plane. CPU and shader
// use this frame so different cube faces decode the same body-frame normal.
struct TangentFrame {
    Vec3d tangent, bitangent;
};
TangentFrame patch_tangent_frame(unsigned face, Vec3d direction);

struct PatchBounds {
    Vec3d centre;              // unit direction
    double angular_radius = 0; // radians, the cap holding every vertex
    double angular_size = 0;   // radians, the cell's edge at its centre
    // Culling sphere radius about the reference-surface centre; includes height and skirts.
    double bound_radius = 0;
};
PatchBounds patch_bounds(PatchKey key);

// Legacy CPU grid plus skirt, in row/edge order. Returns the maximum quad-centre
// deviation from the bilinear corner surface in radii, including curvature.
float generate_patch(const MinorPlanetTerrain& terrain, PatchKey key, std::span<geometry::Vertex> out);
// The index triples every patch shares, patch_index_count of them.
std::vector<std::uint32_t> patch_indices();

// Height-only error at quad centres against bilinear corner heights, in radii.
float patch_error(const MinorPlanetTerrain& terrain, PatchKey key);

// Heights: tile_side squared floats in radii. Colour tiles sample terrain at their
// own resolution: RGBA8 linear albedo and RG16_UNORM tangent slopes over +/-tile_slope_scale.
void generate_height_tile(const MinorPlanetTerrain& terrain, PatchKey key, std::span<float> out);
// Bounds of the rendered height tile, including r16_unorm quantization and float
// decode roundoff. Bilinear sampling and grid morphing stay within this interval.
// These are not bounds on unsampled terrain in a finer descendant.
Range<float> height_tile_range(std::span<const float> heights);
void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<std::uint8_t> albedo,
                           std::span<std::uint16_t> slope, float detail = 1);

// Shared tile_side squared grid: position = (x, y, skirt), x/y in 0..tile_side-1.
// skirt is 0 on the grid and 1 on the perimeter drop ring.
geometry::Mesh patch_grid_mesh();

} // namespace space
