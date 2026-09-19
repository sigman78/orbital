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
    constexpr unsigned quads = tile_side - 1; // 32
    constexpr unsigned half = quads / 2;
    constexpr unsigned span = half + 1; // vertices along one quadrant's edge
    // Every quadrant owns each vertex it uses, the row and column it shares with its neighbours
    // included. A patch draws only the quadrants its children do not, and the shader masks one
    // away by dropping its vertices -- but a shared vertex is needed by the quadrant across it,
    // and the one at the centre by all four. With a single copy the quad diagonally across the
    // centre has three corners nothing may drop, and one of its two triangles outlives a
    // quadrant that is not drawn. Owning them costs 71 vertices of 1292 and keeps the same
    // triangulation everywhere; `u` carries the owner, which the renderer hands the shader.
    geometry::Mesh mesh;
    mesh.vertices.reserve(4 * span * span + 8 * span);
    for (unsigned quadrant = 0; quadrant < 4; quadrant++)
        for (unsigned y = 0; y < span; y++)
            for (unsigned x = 0; x < span; x++)
                mesh.vertices.push_back(
                    {.position = {float((quadrant & 1) * half + x), float((quadrant >> 1) * half + y), 0},
                     .u = float(quadrant)});
    // A grid point as the given quadrant holds it, and the quadrant a cell belongs to, taken
    // from a point inside the cell so the centre row and column never decide it.
    const auto grid = [](unsigned quadrant, unsigned x, unsigned y) {
        return std::uint32_t(quadrant * span * span + (y - (quadrant >> 1) * half) * span +
                             (x - (quadrant & 1) * half));
    };
    const auto quadrant_at = [](float x, float y) { return (x > half ? 1u : 0u) | (y > half ? 2u : 0u); };
    mesh.indices.reserve((quads * quads + 4 * quads) * 6);
    for (unsigned y = 0; y < quads; y++)
        for (unsigned x = 0; x < quads; x++) {
            const unsigned q = quadrant_at(x + .5f, y + .5f);
            const std::uint32_t a = grid(q, x, y), b = grid(q, x + 1, y), c = grid(q, x + 1, y + 1),
                                d = grid(q, x, y + 1);
            mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
        }
    // The skirt ribbon, split at the middle of each side so every half belongs to one quadrant.
    const auto edge = [](unsigned side, unsigned along) {
        switch (side) {
        case 0: return std::pair{along, 0u};
        case 1: return std::pair{quads, along};
        case 2: return std::pair{quads - along, quads};
        default: return std::pair{0u, quads - along};
        }
    };
    for (unsigned side = 0; side < 4; side++)
        for (unsigned part = 0; part < 2; part++) {
            const auto [x0, y0] = edge(side, part * half);
            const auto [x1, y1] = edge(side, part * half + 1);
            const unsigned q = quadrant_at((x0 + x1) * .5f, (y0 + y1) * .5f);
            const std::uint32_t base = std::uint32_t(mesh.vertices.size());
            for (unsigned i = 0; i <= half; i++) {
                const auto [x, y] = edge(side, part * half + i);
                mesh.vertices.push_back({.position = {float(x), float(y), 1}, .u = float(q)});
            }
            for (unsigned i = 0; i < half; i++) {
                const auto [x, y] = edge(side, part * half + i);
                const auto [nx, ny] = edge(side, part * half + i + 1);
                const std::uint32_t a = grid(q, x, y), b = grid(q, nx, ny), a2 = base + i, b2 = a2 + 1;
                mesh.indices.insert(mesh.indices.end(), {a, a2, b, b, a2, b2});
            }
        }
    return mesh;
}

} // namespace space
