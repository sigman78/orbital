#include "scene/terrain_patch.hpp"

#include "core/panic.hpp"

#include <algorithm>
#include <cmath>

namespace space {

namespace {

// Each face's axis with the in-face directions of s and t, chosen so that
// s cross t is the axis: a quad's triangles then wind outward.
struct Face {
    Vec3d axis, s, t;
};
constexpr Face faces[6] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
                           {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
                           {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};

// The cell's span of the face square.
struct Cell {
    double s0, t0, size;
};
Cell cell_of(PatchKey key) {
    const double size = 2.0 / double(1u << key.level);
    return {-1 + key.x * size, -1 + key.y * size, size};
}

} // namespace

Vec3d cube_direction(unsigned face, double s, double t) {
    const Face& f = faces[face];
    return normalized(f.axis + f.s * std::tan(s * pi<double> / 4) + f.t * std::tan(t * pi<double> / 4));
}

PatchBounds patch_bounds(PatchKey key) {
    const Cell cell = cell_of(key);
    PatchBounds bounds{.centre = cube_direction(key.face, cell.s0 + cell.size / 2, cell.t0 + cell.size / 2)};
    for (unsigned corner = 0; corner < 4; corner++) {
        const Vec3d d = cube_direction(key.face, cell.s0 + (corner & 1) * cell.size,
                                       cell.t0 + (corner >> 1) * cell.size);
        bounds.angular_radius = std::max(bounds.angular_radius,
                                         std::acos(std::clamp(dot(d, bounds.centre), -1.0, 1.0)));
    }
    bounds.angular_size = cell.size * pi<double> / 4; // the warp's slope at the face centre
    bounds.angular_radius += 1e-6;
    return bounds;
}

float generate_patch(const MinorPlanetTerrain& terrain, PatchKey key, std::span<geometry::Vertex> out) {
    ORBITAL_ASSERT(out.size() == patch_vertex_count);
    const Cell cell = cell_of(key);
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre, bounds.angular_radius);
    // The skirt drops by the gap a coarser neighbour can leave: its chord's
    // sagitta and the terrain's change over one of its quads.
    const float skirt = float(std::clamp(bounds.angular_size * .5, .001, .1));
    const auto surface = [&](double x, double y) {
        const Vec3d d = cube_direction(key.face, cell.s0 + x * cell.size / patch_quads,
                                       cell.t0 + y * cell.size / patch_quads);
        return d * double(1 + terrain.height(d, region));
    };
    const auto vertex = [&](unsigned x, unsigned y, float drop) {
        const Vec3d p = surface(x, y);
        return geometry::Vertex{.position = to_float(p * (1 - double(drop) / length(p))),
                                .normal = to_float(normalized(p))};
    };
    unsigned n = 0;
    for (unsigned y = 0; y < patch_side; y++)
        for (unsigned x = 0; x < patch_side; x++)
            out[n++] = vertex(x, y, 0);
    // The skirt's edges in the order the indices expect: along the bottom,
    // up the right, back along the top, down the left.
    for (unsigned i = 0; i < patch_side; i++)
        out[n++] = vertex(i, 0, skirt);
    for (unsigned i = 0; i < patch_side; i++)
        out[n++] = vertex(patch_quads, i, skirt);
    for (unsigned i = 0; i < patch_side; i++)
        out[n++] = vertex(patch_quads - i, patch_quads, skirt);
    for (unsigned i = 0; i < patch_side; i++)
        out[n++] = vertex(0, patch_quads - i, skirt);
    ORBITAL_ASSERT(n == patch_vertex_count);
    // The error: the terrain at each quad's centre against the mean of its corners.
    double error = 0;
    for (unsigned y = 0; y < patch_quads; y++)
        for (unsigned x = 0; x < patch_quads; x++) {
            const auto corner = [&](unsigned dx, unsigned dy) {
                return to_double(out[(y + dy) * patch_side + x + dx].position);
            };
            const Vec3d mean = (corner(0, 0) + corner(1, 0) + corner(0, 1) + corner(1, 1)) * .25;
            error = std::max(error, length(surface(x + .5, y + .5) - mean));
        }
    return float(error);
}

std::vector<std::uint32_t> patch_indices() {
    std::vector<std::uint32_t> indices;
    indices.reserve(patch_index_count);
    const auto grid = [](unsigned x, unsigned y) { return y * patch_side + x; };
    for (unsigned y = 0; y < patch_quads; y++)
        for (unsigned x = 0; x < patch_quads; x++) {
            const std::uint32_t a = grid(x, y), b = grid(x + 1, y), c = grid(x + 1, y + 1), d = grid(x, y + 1);
            indices.insert(indices.end(), {a, b, c, a, c, d});
        }
    // Each skirt quad joins two edge vertices to their dropped copies, wound
    // to face away from the patch.
    const auto edge = [&](unsigned i) -> std::uint32_t {
        const unsigned side = i / patch_side, along = i % patch_side;
        switch (side) {
        case 0: return grid(along, 0);
        case 1: return grid(patch_quads, along);
        case 2: return grid(patch_quads - along, patch_quads);
        default: return grid(0, patch_quads - along);
        }
    };
    for (unsigned side = 0; side < 4; side++)
        for (unsigned along = 0; along < patch_quads; along++) {
            const unsigned i = side * patch_side + along;
            const std::uint32_t a = edge(i), b = edge(i + 1);
            const std::uint32_t a2 = patch_side * patch_side + i, b2 = a2 + 1;
            indices.insert(indices.end(), {a, a2, b, b, a2, b2});
        }
    ORBITAL_ASSERT(indices.size() == patch_index_count);
    return indices;
}

} // namespace space
