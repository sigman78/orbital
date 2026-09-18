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

constexpr double everitt_k = 0.8687;
const double tan_k = std::tan(everitt_k);

} // namespace

Vec3d cube_direction(unsigned face, double s, double t) {
    const Face& f = faces[face];
    return normalized(f.axis + f.s * (std::tan(everitt_k * s) / tan_k) + f.t * (std::tan(everitt_k * t) / tan_k));
}

CubeCoord cube_coordinates(Vec3d direction) {
    const double ax = std::abs(direction.x), ay = std::abs(direction.y), az = std::abs(direction.z);
    unsigned face;
    if (ax >= ay && ax >= az)
        face = direction.x > 0 ? 0 : 1;
    else if (ay >= az)
        face = direction.y > 0 ? 2 : 3;
    else
        face = direction.z > 0 ? 4 : 5;
    const Face& f = faces[face];
    const double d = dot(f.axis, direction);
    const double u = dot(f.s, direction) / d;
    const double v = dot(f.t, direction) / d;
    return {face, std::atan(u * tan_k) / everitt_k, std::atan(v * tan_k) / everitt_k};
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
    bounds.angular_size = 2 * std::atan(std::tan(everitt_k * cell.size / 2) / tan_k);
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

float patch_error(const MinorPlanetTerrain& terrain, PatchKey key) {
    const Cell cell = cell_of(key);
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre, bounds.angular_radius);
    constexpr unsigned quads = tile_side - 1;
    const auto h = [&](double x, double y) {
        const Vec3d d = cube_direction(key.face, cell.s0 + x * cell.size / quads, cell.t0 + y * cell.size / quads);
        return length(d) * (1 + terrain.height(d, region));
    };
    double error = 0;
    for (unsigned y = 0; y < quads; y++)
        for (unsigned x = 0; x < quads; x++) {
            const double mean = (h(x, y) + h(x + 1, y) + h(x, y + 1) + h(x + 1, y + 1)) * .25;
            error = std::max(error, std::abs(h(x + .5, y + .5) - mean));
        }
    return float(error);
}

void generate_height_tile(const MinorPlanetTerrain& terrain, PatchKey key, std::span<float> out) {
    ORBITAL_ASSERT(out.size() == tile_side * tile_side);
    const Cell cell = cell_of(key);
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre, bounds.angular_radius);
    constexpr unsigned quads = tile_side - 1;
    for (unsigned y = 0; y < tile_side; y++)
        for (unsigned x = 0; x < tile_side; x++) {
            const Vec3d d = cube_direction(key.face, cell.s0 + double(x) * cell.size / quads,
                                           cell.t0 + double(y) * cell.size / quads);
            out[y * tile_side + x] = terrain.height(d, region);
        }
}

void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<const float> heights,
                           std::span<std::uint8_t> albedo, std::span<std::uint8_t> normal) {
    ORBITAL_ASSERT(heights.size() == tile_side * tile_side);
    ORBITAL_ASSERT(albedo.size() == tile_side * tile_side * 4);
    ORBITAL_ASSERT(normal.size() == tile_side * tile_side * 4);
    const Cell cell = cell_of(key);
    constexpr unsigned quads = tile_side - 1, bordered = tile_side + 2;
    const float range = MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min;
    const auto direction = [&](int x, int y) {
        return cube_direction(key.face, cell.s0 + x * cell.size / quads, cell.t0 + y * cell.size / quads);
    };
    // The tile's heights with a one-texel ring around them, sampled here, so an
    // edge texel's central difference is the one its neighbour tile computes.
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre,
                                                             bounds.angular_radius + 2 * bounds.angular_size / quads);
    std::vector<float> h(bordered * bordered);
    const auto at = [&](int x, int y) -> float& { return h[(y + 1) * bordered + (x + 1)]; };
    for (int y = -1; y <= int(tile_side); y++)
        for (int x = -1; x <= int(tile_side); x++)
            at(x, y) = x >= 0 && y >= 0 && x < int(tile_side) && y < int(tile_side)
                           ? heights[y * tile_side + x]
                           : terrain.height(direction(x, y), region);
    for (int y = 0; y < int(tile_side); y++)
        for (int x = 0; x < int(tile_side); x++) {
            const unsigned i = y * tile_side + x;
            const Vec3d d = direction(x, y);
            const float hc = at(x, y);
            // Slopes per unit of arc: the height change over the chord between the two neighbours.
            const float dx = (at(x + 1, y) - at(x - 1, y)) / float(length(direction(x + 1, y) - direction(x - 1, y)));
            const float dy = (at(x, y + 1) - at(x, y - 1)) / float(length(direction(x, y + 1) - direction(x, y - 1)));
            const Vec3f n = normalized(Vec3f{-dx, -dy, 1});
            const Vec3f a = terrain.albedo(d, hc, 1 - n.z); // the slope term as the bake passes it
            const auto u8 = [](float v) { return std::uint8_t(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f)); };
            const std::size_t p = i * 4;
            albedo[p] = u8(a.x);
            albedo[p + 1] = u8(a.y);
            albedo[p + 2] = u8(a.z);
            albedo[p + 3] = 255;
            normal[p] = u8(n.x * .5f + .5f);
            normal[p + 1] = u8(n.y * .5f + .5f);
            normal[p + 2] = u8(n.z * .5f + .5f);
            normal[p + 3] = u8((hc - MinorPlanetTerrain::height_min) / range);
        }
}

geometry::Mesh patch_grid_mesh() {
    constexpr unsigned quads = tile_side - 1;
    constexpr unsigned grid_count = tile_side * tile_side;
    constexpr unsigned skirt_count = 4 * tile_side;
    geometry::Mesh mesh;
    mesh.vertices.resize(grid_count + skirt_count);
    for (unsigned y = 0; y < tile_side; y++)
        for (unsigned x = 0; x < tile_side; x++)
            mesh.vertices[y * tile_side + x] = {.position = {float(x), float(y), 0}};
    unsigned n = grid_count;
    for (unsigned i = 0; i < tile_side; i++)
        mesh.vertices[n++] = {.position = {float(i), 0, 1}};
    for (unsigned i = 0; i < tile_side; i++)
        mesh.vertices[n++] = {.position = {float(quads), float(i), 1}};
    for (unsigned i = 0; i < tile_side; i++)
        mesh.vertices[n++] = {.position = {float(quads - i), float(quads), 1}};
    for (unsigned i = 0; i < tile_side; i++)
        mesh.vertices[n++] = {.position = {0, float(quads - i), 1}};
    ORBITAL_ASSERT(n == grid_count + skirt_count);
    const auto grid = [](unsigned x, unsigned y) -> std::uint32_t { return y * tile_side + x; };
    mesh.indices.reserve((quads * quads + 4 * quads) * 6);
    for (unsigned y = 0; y < quads; y++)
        for (unsigned x = 0; x < quads; x++) {
            const std::uint32_t a = grid(x, y), b = grid(x + 1, y), c = grid(x + 1, y + 1), d = grid(x, y + 1);
            mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
        }
    const auto edge = [&](unsigned i) -> std::uint32_t {
        const unsigned side = i / tile_side, along = i % tile_side;
        switch (side) {
        case 0: return grid(along, 0);
        case 1: return grid(quads, along);
        case 2: return grid(quads - along, quads);
        default: return grid(0, quads - along);
        }
    };
    for (unsigned side = 0; side < 4; side++)
        for (unsigned along = 0; along < quads; along++) {
            const unsigned i = side * tile_side + along;
            const std::uint32_t a = edge(i), b = edge(i + 1);
            const std::uint32_t a2 = grid_count + i, b2 = a2 + 1;
            mesh.indices.insert(mesh.indices.end(), {a, a2, b, b, a2, b2});
        }
    return mesh;
}

} // namespace space
