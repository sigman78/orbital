#pragma once
#include "scene/geometry.hpp"
#include "scene/terrain.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace space {

// The minor planet's near tier is a cube sphere: six faces, each a quadtree of
// square patches, every patch the same grid of quads over its cell of the face
// with a skirt hanging from its edges to hide the cracks between levels. A
// patch's vertices are in the body's local frame, in radii, so they draw
// through the surface vertex shader like the sphere levels; their normals are
// the sphere's, so the maps shade them exactly as they shade the far tier.

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
constexpr unsigned patch_level_max = 10;  // a cell of 90 degrees over 1024, quads of 0.1 mrad
constexpr unsigned tile_side = 65;        // texels per tile edge, covering 64 quads
constexpr unsigned tile_colour_ratio = 1; // colour texels per height texel (1 for now; 2 later for extra detail)

// Direction of a face point, s and t in [-1, 1], warped by Everitt's mapping
// (tan(k*s)/tan(k), k=0.8687) so texel areas stay within about 11 percent.
Vec3d cube_direction(unsigned face, double s, double t);

struct CubeCoord {
    unsigned face;
    double s, t; // in [-1, 1]
};
CubeCoord cube_coordinates(Vec3d direction);

struct PatchBounds {
    Vec3d centre;              // unit direction
    double angular_radius = 0; // radians, the cap holding every vertex
    double angular_size = 0;   // radians, the cell's edge at its centre
};
PatchBounds patch_bounds(PatchKey key);

// The patch's vertices, patch_vertex_count of them: the grid row by row from
// the cell's low corner, then the skirt around it. Returns the patch's
// geometric error: the farthest the terrain at a quad's centre lies from the
// quad's bilinear surface, in radii, the sphere's own curvature included; the
// tier splits a patch while that error projects larger than its tolerance.
float generate_patch(const MinorPlanetTerrain& terrain, PatchKey key, std::span<geometry::Vertex> out);
// The index triples every patch shares, patch_index_count of them.
std::vector<std::uint32_t> patch_indices();

// The patch's geometric error in radii, from the terrain at quad centres against
// the bilinear surface of the tile's corner heights, the sphere's curvature included.
float patch_error(const MinorPlanetTerrain& terrain, PatchKey key);

// Tile generation: heights in radii (tile_side² floats), then colour tiles
// (tile_side² × 4 bytes each for albedo RGBA8 and normal+height RGBA8).
void generate_height_tile(const MinorPlanetTerrain& terrain, PatchKey key, std::span<float> out);
void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<const float> heights,
                           std::span<std::uint8_t> albedo, std::span<std::uint8_t> normal);

// The shared grid mesh for all patches: 65×65 vertices whose position holds
// (x, y, skirt), x and y in 0..64, skirt 0 on the grid and 1 on the drop ring.
geometry::Mesh patch_grid_mesh();

} // namespace space
