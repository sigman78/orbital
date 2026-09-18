#pragma once
#include "scene/geometry.hpp"
#include "scene/terrain.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace space {

// Cube-sphere quadtree patches in body-local radii, with skirts for seam fallback.

struct PatchKey {
    std::uint8_t face = 0, level = 0;
    std::uint16_t x = 0, y = 0; // the cell within the face at this level, 0 .. 2^level - 1
    constexpr bool operator==(const PatchKey&) const = default;
    // Layout: face [0:2], level [3:7], x [8:19], y [20:31]. Every term is widened first:
    // x and y promote to signed int, and y << 20 overflows it past level 11.
    constexpr std::uint32_t packed() const {
        return std::uint32_t(face) | std::uint32_t(level) << 3 | std::uint32_t(x) << 8 | std::uint32_t(y) << 20;
    }
    constexpr PatchKey child(unsigned i) const {
        return {face, std::uint8_t(level + 1), std::uint16_t(2 * x + (i & 1)), std::uint16_t(2 * y + (i >> 1))};
    }
};

constexpr unsigned patch_level_max = 11; // a cell of 90 degrees over 2048, quads of 24 urad

// The deepest level decides every field that carries a cell index, so raising it silently
// truncates rather than failing. Each of these is the real ceiling, not a restatement of it.
constexpr unsigned patch_cells_max = 1u << patch_level_max; // cells per face edge at the deepest level
static_assert(patch_cells_max - 1 <= 0xfff, "a cell index must fit packed()'s 12-bit x and y fields");
static_assert(patch_level_max <= 0x1f, "the level must fit packed()'s 5-bit field");
static_assert(patch_cells_max - 1 <= 0xffff, "PatchKey stores x and y as uint16_t");
static_assert(patch_level_max < 32, "edge flags shift by 1u << level");

constexpr unsigned tile_side = 33; // texels per tile edge, covering 32 quads
// Colour samples per height interval; memory scales with the square of this ratio.
constexpr unsigned tile_colour_ratio = 2;
constexpr unsigned tile_colour_side = (tile_side - 1) * tile_colour_ratio + 1;
// Slope encoding range in radii/radian, including terrain and micro-relief.
constexpr float tile_slope_scale = 3.f;
// Radial skirt drop, also included in culling bounds.
constexpr float patch_skirt_drop = .002f;

// Everitt cube mapping: s/t in [-1,1], warp tan(0.8687*s)/tan(0.8687).
Vec3d cube_direction(unsigned face, double s, double t);

struct CubeCoord {
    unsigned face;
    double s, t; // in [-1, 1]
};
CubeCoord cube_coordinates(Vec3d direction);

// CPU/shader tangent frame: project the face s-axis onto the sphere tangent plane.
struct TangentFrame {
    Vec3d tangent, bitangent;
};
TangentFrame patch_tangent_frame(unsigned face, Vec3d direction);

struct PatchBounds {
    Vec3d centre;              // unit direction
    double angular_radius = 0; // radians, the cap holding every vertex
    double angular_size = 0;   // radians, the cell's edge at its centre
};
PatchBounds patch_bounds(PatchKey key);

// The culling sphere over a height interval, the skirt drop included. Its centre sits on the
// patch's own shell, not the reference surface: a tile lying 0.03 radii below that surface
// would otherwise spend 0.03 of bound before reaching any of its own geometry.
struct PatchSphere {
    double offset = 1; // the centre is the cap's, scaled by this
    double radius = 0; // radii
};
PatchSphere patch_cull_sphere(const PatchBounds& bounds, Range<float> heights);

// Height-only error at quad centres against bilinear corner heights, in radii.
float patch_error(const MinorPlanetTerrain& terrain, PatchKey key);

// Outputs: tile_side squared heights; colour-side RGBA8 albedo and RG16_UNORM tangent slopes.
void generate_height_tile(const MinorPlanetTerrain& terrain, PatchKey key, std::span<float> out);
// Bounds bilinear tile heights including quantization, but not unsampled descendants.
Range<float> height_tile_range(std::span<const float> heights);
void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<std::uint8_t> albedo,
                           std::span<std::uint16_t> slope, float detail = 1);

// Shared grid positions: (x, y, skirt), x/y in 0..tile_side-1; skirt=1 on the drop ring.
geometry::Mesh patch_grid_mesh();

} // namespace space
