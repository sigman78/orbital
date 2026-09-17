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
constexpr unsigned patch_level_max = 10; // a cell of 90 degrees over 1024, quads of 0.1 mrad

// Direction of a face point, s and t in [-1, 1], the square warped by the
// tangent so cells are near-uniform on the sphere.
Vec3d cube_direction(unsigned face, double s, double t);

struct PatchBounds {
    Vec3d centre;              // unit direction
    double angular_radius = 0; // radians, the cap holding every vertex
    double angular_size = 0;   // radians, the cell's edge at its centre
};
PatchBounds patch_bounds(PatchKey key);

// The patch's vertices, patch_vertex_count of them: the grid row by row from
// the cell's low corner, then the skirt around it.
void generate_patch(const MinorPlanetTerrain& terrain, PatchKey key, std::span<geometry::Vertex> out);
// The index triples every patch shares, patch_index_count of them.
std::vector<std::uint32_t> patch_indices();

} // namespace space
