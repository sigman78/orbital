#pragma once
#include "core/math.hpp"
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
    // The cell one level up that contains this one; a root is its own parent.
    constexpr PatchKey parent() const {
        return level ? PatchKey{face, std::uint8_t(level - 1), std::uint16_t(x / 2), std::uint16_t(y / 2)} : *this;
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
    double angular_radius = 0; // radians, the cap holding every vertex, for the cap distances
    double angular_size = 0;   // radians, the cell's edge at its centre
    // The cell itself: four corner directions around it, and the inward normals of the four
    // planes through the origin that bound it. A cell's s = s0 boundary lies in the plane of
    // the face axis offset by the warp and the face's t axis, so the cell is a convex cone.
    Vec3d corners[4];
    Vec3d edges[4]; // edges[i] bounds the arc from corners[i] to corners[(i + 1) & 3]
};
PatchBounds patch_bounds(PatchKey key);

// Largest dot(normal, point) over the patch's shell: every direction of its cell at every
// radius of the interval, the skirt drop included. A patch lies entirely outside a plane when
// its support falls below the plane's own offset, which is the whole plane test.
//
// This is exact for the cell: the maximum over a convex cone is the normal itself where it
// points inside, and otherwise lies on the boundary, at a corner or on one edge arc. A cap
// around the cell reaches its angular radius in every direction while the cell is a factor of
// root two closer along its edges, and what that gives away is 0.085 radii at level 2 and
// 0.025 at level 4 -- tens of kilometres on this body, which is enough to keep a patch that
// far outside the frustum. It is dearer per test, and still the cheaper cull: a cap pre-test
// ahead of it only rejected what it rejects too, and cost more than it saved.
double patch_cell_support(const PatchBounds& bounds, Vec3d normal, Range<float> heights);

// Height-only error at quad centres against bilinear corner heights, in radii.
float patch_error(const MinorPlanetTerrain& terrain, PatchKey key);

// Outputs: tile_side squared heights; colour-side RGBA8 albedo and RG16_UNORM tangent slopes.
void generate_height_tile(const MinorPlanetTerrain& terrain, PatchKey key, std::span<float> out);
// Bounds bilinear tile heights including quantization, but not unsampled descendants.
Range<float> height_tile_range(std::span<const float> heights);
void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<std::uint8_t> albedo,
                           std::span<std::uint16_t> slope, float detail = 1);

// One vertex of the shared patch grid. It is not a mesh vertex: it carries no position of its
// own, only where it sits on the tile's lattice and which quadrant owns it, and the shaders
// build the rest. The owner is what lets a quadrant be masked away -- see patch_grid.
struct PatchVertex {
    std::uint8_t x = 0, y = 0; // the lattice point, 0 .. tile_side - 1
    std::uint8_t quadrant = 0; // whose it is; it goes when that quadrant is not drawn
    bool skirt = false;        // on the ring that hangs below the tile's border
};

struct PatchGrid {
    std::vector<PatchVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// The grid every patch is drawn from: one tile's worth of quads, a skirt ring around it, and
// each quadrant holding its own copy of the row and column it shares with its neighbours.
PatchGrid patch_grid();

} // namespace space
