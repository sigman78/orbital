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

TangentFrame patch_tangent_frame(unsigned face, Vec3d direction) {
    const Vec3d d = normalized(direction);
    const Vec3d tangent = normalized(faces[face].s - d * dot(faces[face].s, d));
    return {tangent, cross(d, tangent)};
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
    // Around the cell, so consecutive corners share an edge.
    const double s[4] = {cell.s0, cell.s0 + cell.size, cell.s0 + cell.size, cell.s0};
    const double t[4] = {cell.t0, cell.t0, cell.t0 + cell.size, cell.t0 + cell.size};
    for (unsigned i = 0; i < 4; i++)
        bounds.corners[i] = cube_direction(key.face, s[i], t[i]);
    for (unsigned i = 0; i < 4; i++) {
        const Vec3d normal = normalized(cross(bounds.corners[i], bounds.corners[(i + 1) & 3]));
        // Inward, so the cell is where all four are non-negative.
        bounds.edges[i] = dot(normal, bounds.centre) < 0 ? normal * -1 : normal;
    }
    return bounds;
}

double patch_cell_support(const PatchBounds& bounds, Vec3d normal, Range<float> heights) {
    const double top = 1 + double(heights.max);
    const double bottom = 1 + double(heights.min) - patch_skirt_drop;
    bool inside = true;
    for (unsigned i = 0; i < 4; i++)
        inside = inside && dot(normal, bounds.edges[i]) >= 0;
    double reach = -1;
    if (inside) {
        reach = 1; // the normal's own direction is in the cell, and nothing beats it
    } else {
        for (const Vec3d& corner : bounds.corners)
            reach = std::max(reach, dot(normal, corner));
        for (unsigned i = 0; i < 4; i++) {
            // The best direction on this edge's great circle is the normal projected into its
            // plane; it only counts when it falls between the two corners it runs between.
            const Vec3d projected = normal - bounds.edges[i] * dot(normal, bounds.edges[i]);
            const double len = length(projected);
            if (len <= 1e-12)
                continue;
            const Vec3d& a = bounds.corners[i];
            const Vec3d& b = bounds.corners[(i + 1) & 3];
            const double span = dot(a, b);
            const Vec3d candidate = projected * (1 / len);
            if (dot(candidate, a) >= span && dot(candidate, b) >= span)
                reach = std::max(reach, len); // dot(normal, candidate) is the projection's length
        }
    }
    return reach >= 0 ? top * reach : bottom * reach;
}

// Use the neighbor's grid beyond cube edges so both faces share derivative stencils.
namespace {
Vec3d ring_direction(unsigned face, double s, double t) {
    const bool out_s = s < -1 || s > 1, out_t = t < -1 || t > 1;
    if (!out_s && !out_t)
        return cube_direction(face, s, t);
    const double sc = std::clamp(s, -1.0, 1.0), tc = std::clamp(t, -1.0, 1.0);
    const Vec3d edge = cube_direction(face, sc, tc);
    const double delta = out_s ? std::abs(s - sc) : std::abs(t - tc);
    const unsigned neighbour = cube_coordinates(cube_direction(face, s, t)).face;
    const Face& f = faces[neighbour];
    const double d = dot(f.axis, edge);
    double sn = std::atan(dot(f.s, edge) / d * tan_k) / everitt_k;
    double tn = std::atan(dot(f.t, edge) / d * tan_k) / everitt_k;
    // Axis alignment avoids the coordinate-magnitude tie at cube corners.
    if (std::abs(dot(f.s, faces[face].axis)) > .5)
        sn -= std::copysign(delta, sn);
    else
        tn -= std::copysign(delta, tn);
    return cube_direction(neighbour, sn, tn);
}
} // namespace

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

Range<float> height_tile_range(std::span<const float> heights) {
    ORBITAL_ASSERT(!heights.empty());
    const auto [low, high] = std::minmax_element(heights.begin(), heights.end());
    constexpr float padding = (MinorPlanetTerrain::height_max - MinorPlanetTerrain::height_min) / 65535.f + 1e-6f;
    return {*low - padding, *high + padding};
}

void generate_colour_tiles(const MinorPlanetTerrain& terrain, PatchKey key, std::span<std::uint8_t> albedo,
                           std::span<std::uint16_t> slope, float detail) {
    ORBITAL_ASSERT(albedo.size() == tile_colour_side * tile_colour_side * 4);
    ORBITAL_ASSERT(slope.size() == tile_colour_side * tile_colour_side * 2);
    const Cell cell = cell_of(key);
    constexpr unsigned quads = tile_colour_side - 1, bordered = tile_colour_side + 2;
    const auto direction = [&](int x, int y) {
        return ring_direction(key.face, cell.s0 + x * cell.size / quads, cell.t0 + y * cell.size / quads);
    };
    // A one-texel border gives adjacent tiles matching central differences.
    const PatchBounds bounds = patch_bounds(key);
    const MinorPlanetTerrain::Region region = terrain.region(bounds.centre,
                                                             bounds.angular_radius + 2 * bounds.angular_size / quads);
    const double nyquist = .5 * quads / bounds.angular_size;
    std::vector<float> h(bordered * bordered);
    const auto at = [&](int x, int y) -> float& { return h[(y + 1) * bordered + (x + 1)]; };
    for (int y = -1; y <= int(tile_colour_side); y++)
        for (int x = -1; x <= int(tile_colour_side); x++) {
            const Vec3d d = direction(x, y);
            at(x, y) = terrain.height(d, region) + terrain.detail(d, nyquist, detail);
        }
    for (int y = 0; y < int(tile_colour_side); y++)
        for (int x = 0; x < int(tile_colour_side); x++) {
            const unsigned i = y * tile_colour_side + x;
            const Vec3d d = direction(x, y);
            const float hc = at(x, y);
            // Solve the gradient in the skewed grid basis.
            const Vec3d us = direction(x + 1, y) - direction(x - 1, y), vt = direction(x, y + 1) - direction(x, y - 1);
            const Vec3d u = normalized(us), v = normalized(vt);
            const double ds = (at(x + 1, y) - at(x - 1, y)) / length(us),
                         dt = (at(x, y + 1) - at(x, y - 1)) / length(vt);
            const double c = dot(u, v), det = std::max(1 - c * c, 1e-6);
            const Vec3d g = u * ((ds - c * dt) / det) + v * ((dt - c * ds) / det);
            const Vec3f a = terrain.albedo(
                d, hc, float(1 - dot(normalized(d - g), d))); // the slope term as the bake passes it
            const auto u8 = [](float v) { return std::uint8_t(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f)); };
            const std::size_t p = i * 4;
            albedo[p] = u8(a.x);
            albedo[p + 1] = u8(a.y);
            albedo[p + 2] = u8(a.z);
            albedo[p + 3] = 255;
            const TangentFrame frame = patch_tangent_frame(key.face, d);
            const auto u16 = [](double s) {
                return std::uint16_t(std::clamp(s / tile_slope_scale * .5 + .5, 0.0, 1.0) * 65535.0 + .5);
            };
            slope[i * 2] = u16(dot(g, frame.tangent));
            slope[i * 2 + 1] = u16(dot(g, frame.bitangent));
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
            // The two quads that meet the grid's centre corner to corner take the other
            // diagonal. A patch draws only the quadrants its children do not, and the shader can
            // mask a quadrant away only by dropping vertices -- but a vertex on the centre row
            // or column is needed by the quadrant across it, and the centre itself by all four.
            // Split these two the usual way and one triangle of each has all three corners on
            // that cross, so nothing can drop it and it outlives a quadrant that is not drawn:
            // a flap over the gap, degenerate at full morph and visible at every phase before.
            // The other diagonal puts the quad's own outer corner in both triangles, and that
            // corner belongs to this quadrant alone. See terrain_tests' test_quadrant_mask.
            const unsigned half = quads / 2;
            if ((x == half && y == half - 1) || (x == half - 1 && y == half))
                mesh.indices.insert(mesh.indices.end(), {a, b, d, b, c, d});
            else
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
